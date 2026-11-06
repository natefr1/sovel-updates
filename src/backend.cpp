#include "backend.h"
#include "release.h"
#include "sovelmode.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QProcessEnvironment>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUrl>
#include <QImageReader>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QLocalSocket>
#include <algorithm>

static bool launchManaged(const QString &program,const QStringList &args){
    QProcess process;
    auto env=QProcessEnvironment::systemEnvironment();
    env.remove("QT_WAYLAND_SHELL_INTEGRATION");
    process.setProcessEnvironment(env);process.setProgram(program);process.setArguments(args);
    return process.startDetached();
}
static void boundProcessLifetime(QProcess *process,int milliseconds=10000){
    auto *timeout=new QTimer(process);timeout->setSingleShot(true);
    QObject::connect(process,&QProcess::started,timeout,[timeout,milliseconds]{timeout->start(milliseconds);});
    QObject::connect(timeout,&QTimer::timeout,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
    QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),timeout,&QTimer::stop);
}

Backend::Backend(QObject *parent):QObject(parent),m_settings("Sovel","Desktop") {
    connect(&m_timer,&QTimer::timeout,this,&Backend::sample);
    m_timer.start(2000); sample();
}
QString Backend::username() const { return qEnvironmentVariable("USER", "User"); }
QString Backend::home() const { return QDir::homePath(); }
bool Backend::reducedMotion() const { return m_settings.value("motion/reduced",false).toBool(); }
bool Backend::lowEffects() const { return m_settings.value("glass/low",false).toBool(); }
void Backend::setReducedMotion(bool v){ m_settings.setValue("motion/reduced",v);emit preferencesChanged(); }
void Backend::setLowEffects(bool v){ m_settings.setValue("glass/low",v);emit preferencesChanged(); }
QString Backend::wallpaper() const {
    // Normal Mode is the everyday city; Private Mode is the white spider emblem.
    const auto fallback=SovelMode::isPrivate()?QStringLiteral("emblem"):QStringLiteral("city");
    const auto value=m_settings.value("appearance/wallpaper",fallback).toString();
    static const QStringList known{"city","emblem","lab","landscape","lockroom"};
    // Earlier releases defaulted to "landscape"; an untouched value follows the mode.
    if(!known.contains(value)||(value=="landscape"&&!m_settings.value("appearance/wallpaperChosen",false).toBool()))return fallback;
    return value;
}
QString Backend::mode()const{return SovelMode::name();}
bool Backend::privateMode()const{return SovelMode::isPrivate();}
bool Backend::privateConfigured()const{return SovelMode::configured();}
bool Backend::switchToNormalDesktop(){
    if(!SovelMode::configured())return false;
    auto call=QDBusMessage::createMethodCall("org.freedesktop.login1","/org/freedesktop/login1/seat/self","org.freedesktop.login1.Seat","SwitchTo");
    call.setArguments({QVariant::fromValue<quint32>(quint32(SovelMode::normalTerminal()))});
    const auto reply=QDBusConnection::systemBus().call(call,QDBus::Block,5000);
    if(reply.type()==QDBusMessage::ErrorMessage){message("Could not return to the desktop.");return false;}
    return true;
}
void Backend::refreshVault(){vaultAction("status");}
void Backend::vaultAction(const QString &action,QString passphrase){
    static const QStringList allowed{"status","init","unlock","lock"};
    if(!SovelMode::isPrivate()||!allowed.contains(action)||(m_vaultBusy&&action!="status")){passphrase.fill(QChar(0));return;}
    if((action=="init"||action=="unlock")&&passphrase.isEmpty()){m_vault["error"]="Enter the Vault passphrase.";emit vaultChanged();return;}
    auto *process=new QProcess(this);
    m_vaultBusy=action!="status";emit vaultChanged();
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,action](int,QProcess::ExitStatus){
        auto result=QJsonDocument::fromJson(process->readAllStandardOutput().left(65536)).object().toVariantMap();
        if(result.isEmpty())result={{"error","The Vault service did not respond."}};
        result["action"]=action;m_vault=result;m_vaultBusy=false;emit vaultChanged();process->deleteLater();
        if(action=="unlock"&&!result.contains("error"))QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--open-folder",result.value("mountpoint").toString()});
    });
    connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){
        if(error!=QProcess::FailedToStart)return;
        m_vault={{"error","The Vault tool is not installed."}};m_vaultBusy=false;emit vaultChanged();process->deleteLater();
    });
    process->start("/usr/bin/sovel-vault",{action});
    if(action=="init"||action=="unlock"){
        auto bytes=passphrase.toUtf8()+'\n';process->write(bytes);bytes.fill('\0');
    }
    passphrase.fill(QChar(0));
    process->closeWriteChannel();
}
void Backend::claimMenu(QObject *menu){
    if(m_openMenu&&m_openMenu!=menu)QMetaObject::invokeMethod(m_openMenu,"close");
    m_openMenu=menu;
}
void Backend::releaseShellInput(){
    if(m_openMenu)QMetaObject::invokeMethod(m_openMenu,"close");
    QLocalSocket socket;socket.connectToServer(QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"/sovel-desktop.sock");
    if(socket.waitForConnected(150)){socket.write("{\"releaseInput\":true}\n");socket.waitForBytesWritten(150);socket.disconnectFromServer();}
}
QUrl Backend::wallpaperUrl() const{
    const auto file=m_settings.value("appearance/photo").toString();
    return QFileInfo(file).isFile()?QUrl::fromLocalFile(file):QUrl("qrc:/assets/"+wallpaper()+".png");
}
bool Backend::setWallpaperFile(const QUrl &url){
    if(!url.isLocalFile()){message("Choose an image on this computer.");return false;}
    QFile source(url.toLocalFile());QImageReader reader(url.toLocalFile());
    const auto size=reader.size();
    if(!reader.canRead()||!size.isValid()||qint64(size.width())*size.height()>32000000||!source.open(QIODevice::ReadOnly)||source.size()>64*1024*1024){message("Choose a supported image up to 32 megapixels and 64 MB.");return false;}
    const auto image=reader.read();if(image.isNull()){message("That image could not be decoded.");return false;}
    const auto data=source.readAll();const auto folder=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/wallpapers";
    if(!QDir().mkpath(folder)){message("Could not create your wallpaper folder.");return false;}
    const auto file=folder+"/"+QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex())+"."+QString::fromLatin1(reader.format());
    QSaveFile output(file);if(!output.open(QIODevice::WriteOnly)||output.write(data)!=data.size()||!output.commit()){message("Could not save this wallpaper.");return false;}
    m_settings.setValue("appearance/photo",file);m_settings.setValue("appearance/external",false);m_settings.sync();launchManaged("/usr/libexec/sovel-wallpaper-stop",{});emit preferencesChanged();return true;
}
bool Backend::externalWallpaper()const{
    if(!m_settings.value("appearance/external",false).toBool())return false;
    const auto sockets=QDir(qEnvironmentVariable("XDG_RUNTIME_DIR")).entryList({"waypaper-mpv-*.sock"},QDir::System|QDir::Files);
    return !sockets.isEmpty();
}
void Backend::restoreExternalWallpaper(){
    if(m_settings.value("appearance/external",false).toBool())launchManaged("/usr/libexec/sovel-waypaper",{"--restore"});
}
void Backend::openWallpaperPicker(){
    m_settings.setValue("appearance/external",true);m_settings.sync();emit preferencesChanged();
    if(!launchManaged("/usr/libexec/sovel-waypaper",{}))message("Waypaper is unavailable. Install the wallpaper tools update.");
}
void Backend::setWallpaper(const QString &v){
    if(v!="landscape" && v!="lockroom" && v!="city" && v!="emblem" && v!="lab")return;
    m_settings.setValue("appearance/wallpaperChosen",true);
    m_settings.setValue("appearance/external",false);launchManaged("/usr/libexec/sovel-wallpaper-stop",{});
    m_settings.remove("appearance/photo");m_settings.setValue("appearance/wallpaper",v);m_settings.sync();emit preferencesChanged();
}
QVariantMap Backend::systemInfo() const {
    return {{"name",QSysInfo::machineHostName()},{"system",SovelRelease::system},
        {"version",SovelRelease::version},{"edition",SovelRelease::edition},{"channel",SovelRelease::channel},
        {"kernel",QSysInfo::kernelVersion()},{"architecture",QSysInfo::currentCpuArchitecture()}};
}
void Backend::message(const QString &v){m_notice=v;emit noticeChanged();}
void Backend::refreshAudio(){
    if(m_audioBusy)return;
    m_audioBusy=true;
    auto *p=new QProcess(this);boundProcessLifetime(p);
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError){
        m_audioBusy=false;m_audio={{"available",false},{"error","Audio service is unavailable."}};emit audioChanged();p->deleteLater();
    });
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){
        m_audioBusy=false;
        const auto output=QString::fromUtf8(p->readAllStandardOutput());
        const auto match=QRegularExpression("Volume: ([0-9.]+)").match(output);
        if(code==0 && match.hasMatch())m_audio={{"available",true},{"volume",qRound(match.captured(1).toDouble()*100)},{"muted",output.contains("[MUTED]")}};
        else m_audio={{"available",false},{"error","No default audio output is available."}};
        emit audioChanged();p->deleteLater();
    });
    p->start("wpctl",{"get-volume","@DEFAULT_AUDIO_SINK@"});
}
void Backend::refreshNetwork(){
    if(m_networkBusy)return;
    m_networkBusy=true;
    auto *p=new QProcess(this);boundProcessLifetime(p);
    auto env=QProcessEnvironment::systemEnvironment();env.insert("LC_ALL","C");p->setProcessEnvironment(env);
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError){m_networkBusy=false;m_networkReport="NetworkManager could not be reached.";emit networkChanged();p->deleteLater();});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){
        m_networkBusy=false;
        if(code!=0)m_networkReport="Connection information is unavailable.";
        else {
            QStringList lines;
            const QMap<QString,QString> labels{{"GENERAL.DEVICE","Device"},{"GENERAL.TYPE","Connection type"},{"GENERAL.STATE","Status"},{"GENERAL.CONNECTION","Profile"},{"IP4.ADDRESS","IPv4 address"},{"IP4.GATEWAY","Gateway"},{"IP4.DNS","DNS"},{"IP6.ADDRESS","IPv6 address"}};
            for(const auto &raw:QString::fromUtf8(p->readAllStandardOutput()).split('\n')){
                const auto split=raw.indexOf(':');if(split<0){if(!lines.isEmpty()&&!lines.last().isEmpty())lines<<"";continue;}
                const auto key=raw.left(split).section('[',0,0);const auto value=raw.mid(split+1).trimmed();
                if(labels.contains(key)&&value!="--")lines<<labels[key]+"  ·  "+value;
            }
            m_networkReport=lines.join('\n').trimmed();if(m_networkReport.isEmpty())m_networkReport="No network devices detected.";
        }
        emit networkChanged();p->deleteLater();
    });
    p->start("nmcli",{"--wait","5","-m","multiline","-f","GENERAL.DEVICE,GENERAL.TYPE,GENERAL.STATE,GENERAL.CONNECTION,IP4.ADDRESS,IP4.GATEWAY,IP4.DNS,IP6.ADDRESS","device","show"});
}
void Backend::refreshPower(){
    if(m_powerBusy)return;m_powerBusy=true;
    auto *p=new QProcess(this);boundProcessLifetime(p);
    auto env=QProcessEnvironment::systemEnvironment();env.insert("LC_ALL","C");p->setProcessEnvironment(env);
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError){m_powerBusy=false;m_power={{"available",false},{"error","Power profiles are unavailable on this system."}};emit powerChanged();p->deleteLater();});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){
        m_powerBusy=false;QStringList profiles;QString current;
        for(const auto &line:QString::fromUtf8(p->readAllStandardOutput()).split('\n')){
            const auto match=QRegularExpression("^\\s*(\\*)?\\s*(balanced|power-saver|performance):\\s*$").match(line);
            if(match.hasMatch()){profiles<<match.captured(2);if(!match.captured(1).isEmpty())current=match.captured(2);}
        }
        if(code==0&&!profiles.isEmpty())m_power={{"available",true},{"profiles",profiles},{"current",current}};
        else m_power={{"available",false},{"error","Power profiles are unavailable on this system."}};
        emit powerChanged();p->deleteLater();
    });p->start("powerprofilesctl",{"list"});
}
void Backend::setPowerProfile(const QString &profile){
    if(m_powerBusy||!m_power.value("profiles").toStringList().contains(profile))return;
    m_powerBusy=true;auto *p=new QProcess(this);boundProcessLifetime(p);
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError){m_powerBusy=false;message("Power profile could not be changed.");p->deleteLater();});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){m_powerBusy=false;if(code!=0)message("The system did not allow this power profile change.");refreshPower();p->deleteLater();});
    p->start("powerprofilesctl",{"set",profile});
}
void Backend::audioCommand(const QStringList &args){
    auto *p=new QProcess(this);boundProcessLifetime(p);
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError){message("Audio control could not start.");p->deleteLater();});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){
        if(code!=0)message("Audio setting could not be changed.");
        // PipeWire applies changes asynchronously after wpctl has exited.
        QTimer::singleShot(200,this,&Backend::refreshAudio);p->deleteLater();
    });p->start("wpctl",args);
}
void Backend::setVolume(int percent){audioCommand({"set-volume","@DEFAULT_AUDIO_SINK@",QString::number(qBound(0,percent,100))+"%"});}
void Backend::setMuted(bool muted){audioCommand({"set-mute","@DEFAULT_AUDIO_SINK@",muted?"1":"0"});}
QVariantMap Backend::shellPreferences()const{return {{"autoHide",m_settings.value("shell/autoHide",true)},{"sensitivity",m_settings.value("shell/sensitivity",8)},{"revealDelay",m_settings.value("shell/revealDelay",80)},{"hideDelay",m_settings.value("shell/hideDelay",450)},{"animationSpeed",m_settings.value("shell/animationSpeed",220)},{"activationEdge",m_settings.value("shell/activationEdge","bottom")}};}
void Backend::setShellPreference(const QString &name,const QVariant &value){
    QVariant saved;
    if(name=="autoHide")saved=value.toBool();
    else if(name=="activationEdge"){if(!QStringList{"bottom","top","left","right"}.contains(value.toString()))return;saved=value.toString();}
    else if(name=="sensitivity")saved=qBound(2,value.toInt(),24);
    else if(name=="revealDelay")saved=qBound(0,value.toInt(),1000);
    else if(name=="hideDelay")saved=qBound(100,value.toInt(),2500);
    else if(name=="animationSpeed")saved=qBound(0,value.toInt(),600);
    else return;
    m_settings.setValue("shell/"+name,saved);m_settings.sync();emit preferencesChanged();
}
void Backend::sample(){
    m_settings.sync();
    const int motion=reducedMotion(),effects=lowEffects();
    const auto pins=dockIds();
    if(motion!=m_previousMotion || effects!=m_previousEffects || wallpaper()!=m_previousWallpaper || wallpaperUrl().toString()!=m_previousPhoto || externalWallpaper()!=m_previousExternalWallpaper || pins!=m_previousPins||m_previousShell!=shellPreferences()){
        m_previousShell=shellPreferences();
        m_previousPhoto=wallpaperUrl().toString();
        m_previousExternalWallpaper=externalWallpaper();
        m_previousPins=pins;
        m_previousWallpaper=wallpaper();
        m_previousMotion=motion;m_previousEffects=effects;emit preferencesChanged();
    }
    QFile cpu("/proc/stat");
    if(cpu.open(QIODevice::ReadOnly)){
        auto v=QString::fromUtf8(cpu.readLine()).simplified().split(' ');
        if(v.size()>=9){
            quint64 total=0; // guest fields duplicate user/nice; exclude them.
            for(int i=1;i<=8;i++) total+=v[i].toULongLong();
            auto idle=v[4].toULongLong()+v[5].toULongLong();
            m_metrics["cpu"]=m_total && total>m_total ? qBound(0.0,100.0*(1.0-double(idle-m_idle)/double(total-m_total)),100.0) : -1;
            m_total=total; m_idle=idle;
        }
    }
    QFile mem("/proc/meminfo");
    if(mem.open(QIODevice::ReadOnly)){
        quint64 total=0,available=0;
        for(const auto &raw:mem.readAll().split('\n')){
            auto line=QString::fromUtf8(raw).simplified().split(' ');
            if(line.size()<2)continue;
            if(line[0]=="MemTotal:") total=line[1].toULongLong();
            if(line[0]=="MemAvailable:")available=line[1].toULongLong();
        }
        m_metrics["ramUsed"]=double(total-available)/1048576.0;
        m_metrics["ramTotal"]=double(total)/1048576.0;
    }
    QStorageInfo disk(QDir::homePath());
    if(disk.isValid()){
        m_metrics["diskFree"]=disk.bytesAvailable()/1073741824.0;
        m_metrics["diskTotal"]=disk.bytesTotal()/1073741824.0;
    }
    // GPU utilization is deliberately absent until a real driver backend exists.
    emit metricsChanged();
}
void Backend::launch(const QString &app){
    if(app=="downloads"){
        if(!launchManaged(QCoreApplication::applicationFilePath(),{"--downloads"}))message("Could not open Downloads.");
        return;
    }
    if(app=="updates"||app=="sound-settings"||app=="display-settings"){
        const auto page=app=="updates"?"updates":app=="sound-settings"?"sound":"displays";
        if(!launchManaged(QCoreApplication::applicationFilePath(),{"--app","settings","--settings-page",page}))message("Could not open Settings.");
        return;
    }
    if(app=="network-config" || app=="bluetooth-config"){
        const QString tool=app=="network-config"?"nm-connection-editor":"blueman-manager";
        if(QStandardPaths::findExecutable(tool).isEmpty() || !launchManaged(tool,{}))message("The connection configuration tool is unavailable.");
        return;
    }
    if(app=="files" || app=="settings" || app=="library" || app=="store" || app=="wallpapers" || app=="motion"){
        if(!launchManaged(QCoreApplication::applicationFilePath(),{"--app",app}))message("Could not open "+app);
        return;
    }
    if(app=="browser"){
        const auto brave=QStandardPaths::findExecutable("brave-browser");
        if(brave.isEmpty())message("Brave is not installed.");
        else launchManaged(brave,{});
    } else if(app=="terminal") {
        const auto foot=QStandardPaths::findExecutable("foot");
        if(foot.isEmpty())message("Terminal backend (foot) is not installed.");
        else launchManaged(foot,{});
    } else if(app=="lock" || app=="exit-private") {
        // sovel-mode confirms the lock before anything else happens and, in
        // Private Mode, returns to the Normal lock screen.
        if(app=="exit-private"&&!SovelMode::isPrivate())return;
        auto *locker=new QProcess(this);
        connect(locker,&QProcess::errorOccurred,this,[this,locker](QProcess::ProcessError error){if(error==QProcess::FailedToStart){message("Secure lock service is unavailable.");locker->deleteLater();}});
        connect(locker,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,locker,app](int code,QProcess::ExitStatus status){
            if(app=="lock"&&(code!=0 || status!=QProcess::NormalExit))message("Secure lock could not be established. Your session may still be unlocked.");
            locker->deleteLater();
        });
        locker->start("/usr/bin/sovel-mode",{app});
    } else if(app=="vault") {
        if(SovelMode::isPrivate()&&!launchManaged(QCoreApplication::applicationFilePath(),{"--app","vault"}))message("The Vault could not be opened.");
    } else if(app=="jared")launchJared({});
}
QStringList Backend::dockIds()const{
    if(m_settings.contains("dock/order"))return m_settings.value("dock/order").toStringList();
    auto ids=QStringList{"sovel:files","sovel:terminal","sovel:settings","sovel:store"};
    if(SovelMode::isPrivate())ids<<"sovel:jared"<<"sovel:vault";
    for(const auto &id:m_settings.value("dock/pins").toStringList())if(!ids.contains(id))ids<<id;
    return ids;
}
bool Backend::isPinned(const QString &id) const { return dockIds().contains(id); }
QVariantList Backend::pinnedApps() const {
    QVariantList result; const auto apps=applications();
    for(const auto &id:dockIds())
        for(const auto &app:apps)if(app.toMap()["id"].toString()==id){result.append(app);break;}
    return result;
}
void Backend::setPinned(const QString &id,bool pinned){
    auto pins=dockIds();
    if(pinned){
        bool valid=false;for(const auto &app:applications())if(app.toMap()["id"].toString()==id)valid=true;
        if(!valid || id=="sovel:library" || pins.contains(id))return;
        pins.append(id);
    }else pins.removeAll(id);
    m_settings.setValue("dock/order",pins);m_settings.setValue("dock/pins",pins);m_settings.sync();emit preferencesChanged();
}
void Backend::setDockOrder(const QStringList &ids){
    const auto current=dockIds();QStringList clean;
    for(const auto &id:ids)if(current.contains(id)&&!clean.contains(id))clean<<id;
    for(const auto &id:current)if(!clean.contains(id))clean<<id;
    m_settings.setValue("dock/order",clean);m_settings.sync();emit preferencesChanged();
}
void Backend::movePinned(const QString &id,int direction){
    auto ids=dockIds();const auto index=ids.indexOf(id);if(index<0)return;
    const auto target=qBound(0,int(index)+(direction<0?-1:1),int(ids.size())-1);ids.move(index,target);setDockOrder(ids);
}
void Backend::launchJared(const QString &prompt){
    if(!SovelMode::isPrivate()){message("This app is not available.");return;}
    if(!launchManaged(QCoreApplication::applicationFilePath(),{"--app","jared","--jared-prompt",prompt.left(2000)}))message("JARED could not be opened.");
}
QVariantList Backend::applications(bool refresh) const {
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(!refresh&&m_applicationCacheTime&&now-m_applicationCacheTime<2000)return m_applicationCache;
    QVariantList result;
    QStringList builtIn{"files","settings","store","terminal"};
    if(SovelMode::isPrivate())builtIn<<"jared"<<"vault";
    for(const auto &id:builtIn){
        QString name=id;name[0]=name[0].toUpper();if(id=="jared")name="JARED";
        result.append(QVariantMap{{"id","sovel:"+id},{"name",name},{"icon","qrc:/assets/icons/"+id+".svg"},{"appIds",QStringList{id=="terminal"?"foot":"sovel-"+id}}});
    }
    QSet<QString> seen;
    QSet<QString> names{"files","settings","terminal","store","app library","jared","vault"};
    const QSet<QString> replaced{"org.gnome.Nautilus.desktop","org.gnome.Settings.desktop","gnome-control-center.desktop","org.gnome.Terminal.desktop","org.gnome.Console.desktop","foot.desktop","footclient.desktop","foot-server.desktop"};
    QStringList roots=QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    roots<<QDir::homePath()+"/.local/share/flatpak/exports/share"<<"/var/lib/flatpak/exports/share"<<"/var/lib/snapd/desktop";
    for(const auto &root:roots){
        QDir base(root+"/applications");
        QDirIterator it(base.path(),{"*.desktop"},QDir::Files,QDirIterator::Subdirectories);
        while(it.hasNext()){
            const auto path=it.next();const auto id=base.relativeFilePath(path).replace('/','-');
            if(seen.contains(id))continue;seen.insert(id);
            if(id.startsWith("sovel-") || replaced.contains(id))continue;
            QSettings desktop(path,QSettings::IniFormat);desktop.beginGroup("Desktop Entry");
            if(desktop.value("Hidden",false).toBool() || desktop.value("NoDisplay",false).toBool() || desktop.value("Type").toString()!="Application")continue;
            auto desktops=desktop.value("OnlyShowIn").toString().split(';',Qt::SkipEmptyParts);
            if(!desktops.isEmpty()&&!desktops.contains("Sovel",Qt::CaseInsensitive))continue;
            if(desktop.value("NotShowIn").toString().split(';',Qt::SkipEmptyParts).contains("Sovel",Qt::CaseInsensitive))continue;
            const auto name=desktop.value("Name").toString();if(name.isEmpty())continue;
            const auto nameKey=name.simplified().toCaseFolded();
            if(names.contains(nameKey))continue;
            const auto executable=desktop.value("TryExec").toString();
            if(!executable.isEmpty() && QStandardPaths::findExecutable(executable).isEmpty())continue;
            names.insert(nameKey);
            const auto icon=desktop.value("Icon").toString();
            QStringList appIds{desktop.value("X-Flatpak").toString(),desktop.value("StartupWMClass").toString(),id.left(id.size()-8)};appIds.removeAll("");appIds.removeDuplicates();
            result.append(QVariantMap{{"id",path},{"name",name},{"appIds",appIds},{"icon","image://apps/"+QString::fromLatin1(QUrl::toPercentEncoding("desktop:"+path))}});
        }
    }
    std::sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){return QString::localeAwareCompare(a.toMap()["name"].toString(),b.toMap()["name"].toString())<0;});
    m_applicationCache=result;m_applicationCacheTime=now;
    return result;
}
void Backend::launchApplication(const QString &id){
    for(const auto &v:applications())if(v.toMap()["id"].toString()==id){
        if(id.startsWith("sovel:")){launch(id.mid(6));return;}
        if(!launchManaged("gio",{"launch",id}))message("Could not launch this application.");
        return;
    }
    message("This application is no longer installed.");
}
QVariantList Backend::files(const QString &path) const{
    QVariantList list; QDir dir(path);
    if(!dir.exists())return list;
    for(const auto &f:dir.entryInfoList(QDir::AllEntries|QDir::NoDotAndDotDot,QDir::DirsFirst|QDir::Name|QDir::IgnoreCase)){
        list.append(QVariantMap{{"name",f.fileName()},{"path",f.absoluteFilePath()},{"folder",f.isDir()},{"size",f.size()},{"suffix",f.suffix()}});
    }
    return list;
}
bool Backend::openFile(const QString &path){
    if(!QFileInfo::exists(path)){message("That file is no longer available.");return false;}
    bool ok=QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()));
    if(!ok)message("No application could open this file.");
    return ok;
}
bool Backend::makeFolder(const QString &parent,const QString &name){
    if(name.trimmed().isEmpty() || name=="." || name==".." || name.contains('/') || name.contains(QChar('\0'))) {message("Choose a valid folder name.");return false;}
    bool ok=QDir(parent).mkdir(name);if(!ok)message("Folder could not be created. Check its name and permissions.");return ok;
}
QVariantList Backend::widgets()const{
    return QJsonDocument::fromJson(m_settings.value("widgets/layout").toByteArray()).array().toVariantList();
}
void Backend::saveWidgets(const QVariantList &list){
    QVariantList clean;
    for(const auto &v:list){auto m=v.toMap();auto t=m.value("type").toString();
        if(t!="clock" && t!="system" && t!="jared" && t!="actions")continue;
        clean.append(QVariantMap{{"type",t},{"x",qBound(0.0,m.value("x").toDouble(),1.0)},{"y",qBound(0.0,m.value("y").toDouble(),1.0)}});
        if(clean.size()>=16)break;
    }
    m_settings.setValue("widgets/layout",QJsonDocument(QJsonArray::fromVariantList(clean)).toJson(QJsonDocument::Compact));
}
void Backend::checkUpdates(){
    if(m_checking)return;
    m_checking=true;m_updateReport="Checking installed APT metadata…";emit updateReportChanged();
    auto *p=new QProcess(this);
    connect(p,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){m_checking=false;m_updateReport="APT could not be started.";emit updateReportChanged();});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){
        m_checking=false;
        m_updateReport=code==0?QString::fromUtf8(p->readAllStandardOutput()).trimmed():"APT metadata check failed.";
        if(m_updateReport.isEmpty())m_updateReport="No upgrades listed in the current local metadata.";
        m_updateReport+="\n\nLocal metadata only. Online refresh and privileged installation are not implemented in this build.";
        emit updateReportChanged();p->deleteLater();
    });
    p->start("apt",{"list","--upgradable"});
}
