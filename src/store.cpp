#include "store.h"
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSaveFile>
#include <QTimer>
#include <memory>
#include <QFileInfo>
#include <QCoreApplication>

Store::Store(QObject *parent):QObject(parent),m_network(this){
    QFile catalog(":/assets/store-catalog.json");
    if(catalog.open(QIODevice::ReadOnly))for(const auto &value:QJsonDocument::fromJson(catalog.readAll()).array()){
        const auto raw=value.toObject().toVariantMap();m_catalog.insert(raw.value("id").toString(),normalize(raw));
    }
    m_jobPoll.setInterval(2000);connect(&m_jobPoll,&QTimer::timeout,this,&Store::pollJobs);
}
Store::~Store(){
    // Close service callbacks while the fields they reference still exist.
    for(auto *reply:findChildren<QNetworkReply*>()){QObject::disconnect(reply,nullptr,this,nullptr);reply->abort();}
    for(auto *process:findChildren<QProcess*>()){
        QObject::disconnect(process,nullptr,this,nullptr);
        if(process->state()!=QProcess::NotRunning){process->kill();process->waitForFinished(1000);}
    }
}
QVariantMap Store::normalize(const QVariantMap &raw)const{
    auto result=raw;
    result["description"]=raw.value("summary");
    result["longDescription"]=raw.value("description");
    const auto releases=raw.value("releases").toList();
    result["version"]=releases.isEmpty()?QString():releases.first().toMap().value("version").toString();
    QVariantList screenshots;
    for(const auto &s:raw.value("screenshots").toList()){
        const auto sizes=s.toMap().value("sizes").toList();QVariantMap best;int distance=100000;
        for(const auto &size:sizes){const auto image=size.toMap();const int diff=qAbs(image.value("width").toInt()-752);if(diff<distance){distance=diff;best=image;}}
        if(best.value("src").toString().startsWith("https://"))screenshots<<best.value("src");
    }
    result["screenshots"]=screenshots;
    result["developer"]=raw.value("developer_name");result["license"]=raw.value("project_license");
    result["verified"]=raw.value("metadata").toMap().value("flathub::verification::verified").toBool();
    if(!result.value("icon").toString().startsWith("https://"))result["icon"]="";
    if(!result.contains("sourceKind"))result["sourceKind"]="flatpak";
    result["sourceLabel"]=result["sourceKind"]=="apt"?"Signed system repository":result["sourceKind"]=="appimage"?"Verified official AppImage":"Flathub";
    return result;
}
void Store::browse(const QString &collection){
    if(busy())return;
    m_results.clear();
    for(const auto &entry:m_catalog){
        if(collection=="Installed"?!m_installed.contains(entry.value("id").toString()):collection=="Updates"?!m_updatable.contains(entry.value("id").toString()):collection!="All Apps"&&!entry.value("collections").toStringList().contains(collection))continue;
        m_results<<entry;
    }
    if(collection!="Updates")m_status=collection=="Installed"?"Your installed apps. Search to find more.":"Apps for Sovel · Catalog and app media from Flathub";emit changed();
}
void Store::select(const QString &id){
    if(!listed(id))return;
    m_selected=id;m_permissions.clear();m_reviewed=false;
    m_details=m_catalog.value(id);
    for(const auto &entry:m_results)if(entry.toMap().value("id").toString()==id)m_details=entry.toMap();
    emit detailsChanged();if(m_details.value("sourceKind","flatpak")=="flatpak")fetchDetails(id);
}
void Store::fetchDetails(const QString &id){
    const auto generation=++m_detailGeneration;m_detailsBusy=true;emit detailsChanged();
    QNetworkRequest request(QUrl("https://flathub.org/api/v2/appstream/"+id));
    request.setTransferTimeout(20000);request.setHeader(QNetworkRequest::UserAgentHeader,"Sovel-Store/0.1");
    auto *reply=m_network.get(request);
    connect(reply,&QNetworkReply::readyRead,this,[reply]{if(reply->bytesAvailable()>4*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,id,generation]{
        const auto data=reply->readAll();
        if(generation==m_detailGeneration){
            m_detailsBusy=false;
            const auto document=QJsonDocument::fromJson(data);const auto raw=document.object().toVariantMap();
            if(reply->error()==QNetworkReply::NoError&&data.size()<=4*1024*1024&&raw.value("id").toString()==id){
                auto app=normalize(raw);app["collections"]=m_catalog.value(id).value("collections");m_catalog[id]=app;m_details=app;
            }else{m_status="Showing saved app information. Connect to the internet to refresh previews.";emit changed();}
            emit detailsChanged();
        }
        reply->deleteLater();
    });
}

void Store::refreshInstalled(){
    if(m_refreshing)return;
    m_refreshing=true;auto *p=new QProcess(this);
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError e){if(e==QProcess::FailedToStart){m_refreshing=false;p->deleteLater();}});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){
        m_refreshing=false;
        if(code==0){
            m_installed.clear();
            for(const auto &line:QString::fromUtf8(p->readAllStandardOutput()).split('\n',Qt::SkipEmptyParts)){
                const auto fields=line.split('\t');const auto id=fields.first();m_installed<<id;
                if(!m_catalog.contains(id))m_catalog[id]={{"id",id},{"name",fields.value(1,id)},{"description","Installed app"},{"icon","image://apps/"+id}};
            }
            const auto data=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            if(QFileInfo::exists(data+"/Sovel/apps/com.storytold.ArtCraft/ArtCraft.AppImage"))m_installed<<"com.storytold.ArtCraft";
            if(QFileInfo::exists("/usr/share/applications/org.kde.kdeconnect.app.desktop"))m_installed<<"org.kde.kdeconnect";
            m_installed.removeDuplicates();emit installedChanged();
        }
        p->deleteLater();
    });
    p->start("flatpak",{"list","--app","--columns=application,name"});
}

bool Store::allowedOperation(const QStringList &args){
    // No public or internal Store operation can uninstall apps or delete a remote.
    const auto verb=args.value(0);
    if(verb=="remote-add")return args==QStringList{"remote-add","--user","--if-not-exists","--from","flathub","https://flathub.org/repo/flathub.flatpakrepo"};
    if(verb=="search")return args.size()==4&&args[1]=="--columns=application,name,description,version,remotes"&&args[2]=="--";
    const QRegularExpression id("^[A-Za-z0-9_-]+(?:\\.[A-Za-z0-9_-]+){2,}$");
    if(!id.match(args.value(args.size()-1)).hasMatch())return false;
    if(verb=="install")return args.size()==6&&args.mid(1,4)==QStringList{"--user","--noninteractive","--assumeyes","flathub"};
    if(verb=="remote-info")return (args.size()==4&&args.mid(1,2)==QStringList{"--user","flathub"})||(args.size()==5&&args.mid(1,3)==QStringList{"--user","--show-metadata","flathub"});
    return false;
}
void Store::run(const QStringList &args,std::function<void(bool,const QString&)> done){
    if(!allowedOperation(args)){m_status="This Store only adds and opens apps. Destructive operations are blocked.";emit changed();return;}
    if(busy())return;
    if(QStandardPaths::findExecutable("flatpak").isEmpty()){m_status="The Flatpak app service is not installed.";emit changed();return;}
    auto *p=new QProcess(this);m_process=p;m_cancelled=false;
    auto *deadline=new QTimer(p);deadline->setSingleShot(true);
    connect(deadline,&QTimer::timeout,p,[p]{p->setProperty("timedOut",true);p->kill();});deadline->start(args.value(0)=="install"?1800000:120000);
    auto env=QProcessEnvironment::systemEnvironment();env.insert("LC_ALL","C");env.insert("TERM","dumb");env.remove("QT_WAYLAND_SHELL_INTEGRATION");
    p->setProcessEnvironment(env);p->setProcessChannelMode(QProcess::MergedChannels);
    auto output=std::make_shared<QByteArray>();
    connect(p,&QProcess::readyReadStandardOutput,this,[this,p,output,args]{
        const auto chunk=p->readAllStandardOutput();output->append(chunk);
        if(args.value(0)=="install"){
            const auto lines=QString::fromUtf8(chunk).split(QRegularExpression("[\\r\\n]"),Qt::SkipEmptyParts);
            if(!lines.isEmpty()){m_status="Installing… "+lines.last().left(220);emit changed();}
        }
        if(output->size()>8*1024*1024){p->terminate();m_status="The app service returned too much data.";}
    });
    connect(p,&QProcess::errorOccurred,this,[this,p](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){m_process=nullptr;m_status="Could not start the app service.";p->deleteLater();emit changed();}
    });
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p,output,done](int code,QProcess::ExitStatus exit){
        output->append(p->readAllStandardOutput());m_process=nullptr;
        if(p->property("timedOut").toBool())m_status="The app service took too long. Check your connection and try again.";
        else if(m_cancelled)m_status="Operation cancelled.";
        else done(code==0 && exit==QProcess::NormalExit,QString::fromUtf8(*output));
        p->deleteLater();emit changed();
    });
    p->start("flatpak",args);emit changed();
}
bool Store::listed(const QString &id)const{
    if(!QRegularExpression("^[A-Za-z0-9_-]+(?:\\.[A-Za-z0-9_-]+){2,}$").match(id).hasMatch())return false;
    return m_catalog.contains(id);
}
void Store::ensureSource(std::function<void()> ready){
    run({"remote-add","--user","--if-not-exists","--from","flathub","https://flathub.org/repo/flathub.flatpakrepo"},[this,ready](bool ok,const QString &text){
        if(ok)ready();else m_status="Could not connect to Flathub: "+text.trimmed().right(1500);
    });
}
void Store::search(const QString &query){
    if(busy() || query.trimmed().isEmpty())return;
    m_selected.clear();m_permissions.clear();m_reviewed=false;m_status="Searching Flathub…";
    ensureSource([this,query]{run({"search","--columns=application,name,description,version,remotes","--",query.trimmed()},[this](bool ok,const QString &text){
        m_results.clear();
        if(!ok){m_status=text.trimmed().left(2000);return;}
        QSet<QString> seen;
        for(const auto &line:text.split('\n')){
            const auto fields=line.split('\t');if(fields.size()<5 || !fields[4].contains("flathub") || seen.contains(fields[0]))continue;
            seen.insert(fields[0]);
            auto entry=m_catalog.value(fields[0]);
            entry.insert("id",fields[0]);entry.insert("name",fields[1]);entry.insert("description",fields[2]);entry.insert("version",fields[3]);
            m_catalog[fields[0]]=entry; m_results.append(entry);
        }
        m_status=m_results.isEmpty()?"No matches. Try another app name.":QString("%1 apps available from Flathub").arg(m_results.size());
    });});
}
void Store::review(const QString &id){
    if(busy() || !listed(id))return;
    m_selected=id;m_permissions.clear();m_reviewed=false;m_status="Reading app permissions…";
    const auto app=m_catalog.value(id);
    if(app.value("sourceKind")=="apt"||app.value("sourceKind")=="appimage"){
        m_permissions=app.value("sourceKind")=="apt"?"Install KDE Connect from your existing signed system repositories. Administrator approval is required. The native app runs with your account's normal access. Package removals are blocked.":"Install the official ArtCraft AppImage for your account. It runs outside a sandbox and has your normal account access. The exact release size and SHA-256 are checked before installation. Additional models are separate downloads.\n\nSource: "+app.value("download").toMap().value("url").toString()+"\nSHA-256: "+app.value("download").toMap().value("sha256").toString();
        m_reviewed=true;emit reviewReady();emit changed();return;
    }
    ensureSource([this,id]{run({"remote-info","--user","--show-metadata","flathub",id},[this](bool ok,const QString &text){
        if(!ok){m_status=text.trimmed().left(2000);return;}
        // Display the exact sandbox metadata before any transaction is started.
        m_permissions=text;
        run({"remote-info","--user","flathub",m_selected},[this](bool infoOk,const QString &info){
            if(infoOk)m_permissions=info+"\n\nSandbox permissions\n"+m_permissions;
            m_reviewed=true;m_status="Review permissions before installing.";emit reviewReady();
        });
    });});
}
void Store::installSelected(){
    if(busy() || !m_reviewed || !listed(m_selected))return;
    m_reviewed=false;startJob("install",m_selected);
}
void Store::cancel(){if(m_process){m_cancelled=true;m_process->terminate();}}
void Store::open(const QString &id){
    if(!listed(id)||!m_installed.contains(id))return;
    QProcess process;auto env=QProcessEnvironment::systemEnvironment();env.remove("QT_WAYLAND_SHELL_INTEGRATION");process.setProcessEnvironment(env);
    const auto kind=m_catalog.value(id).value("sourceKind").toString();
    if(kind=="apt"){process.setProgram("gio");process.setArguments({"launch","/usr/share/applications/org.kde.kdeconnect.app.desktop"});}
    else if(kind=="appimage"){process.setProgram("gio");process.setArguments({"launch",QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/applications/com.storytold.ArtCraft.desktop"});}
    else {process.setProgram("flatpak");process.setArguments({"run",id});}
    // Launched apps belong to the session, not to the Store window's lifetime.
    m_status=process.startDetached()?"Opening "+id+"…":"Could not launch this app.";emit changed();
}
void Store::worker(const QStringList &args,std::function<void(bool,const QVariantMap&)> done){
    auto p=new QProcess(this);auto output=std::make_shared<QByteArray>();auto deadline=new QTimer(p);deadline->setSingleShot(true);deadline->start(120000);connect(deadline,&QTimer::timeout,p,&QProcess::kill);
    connect(p,&QProcess::readyReadStandardOutput,this,[p,output]{output->append(p->readAllStandardOutput());if(output->size()>2*1024*1024)p->kill();});
    auto finished=std::make_shared<bool>(false);auto complete=[p,output,done,finished](bool ok){if(*finished)return;*finished=true;output->append(p->readAllStandardOutput());const auto doc=QJsonDocument::fromJson(*output);done(ok&&doc.isObject(),doc.object().toVariantMap());p->deleteLater();};
    connect(p,&QProcess::errorOccurred,this,[complete](QProcess::ProcessError error){if(error==QProcess::FailedToStart)complete(false);});
    connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[complete](int code,QProcess::ExitStatus status){complete(code==0&&status==QProcess::NormalExit);});
    const QString helper=QFileInfo::exists("/usr/libexec/sovel-store-worker")?"/usr/libexec/sovel-store-worker":QCoreApplication::applicationDirPath()+"/../packaging/sovel-store-worker";
    p->start("python3",QStringList{helper}+args);
}
void Store::pollJobs(){
    if(m_pollingJobs)return;m_pollingJobs=true;
    worker({"status"},[this](bool ok,const QVariantMap &reply){
        m_pollingJobs=false;if(!ok)return;const auto jobs=reply.value("jobs").toList();if(jobs==m_jobs)return;
        bool completed=false;
        for(auto value:jobs){const auto job=value.toMap();if(job.value("state")!="complete")continue;bool seen=false;
            for(auto previous:m_jobs){auto old=previous.toMap();if(old.value("job")==job.value("job")&&old.value("state")=="complete")seen=true;}
            if(!seen)completed=true;
        }
        m_jobs=jobs;emit jobsChanged();if(completed){refreshInstalled();checkAppUpdates();}
    });
}
void Store::startJob(const QString &operation,const QString &id,const QString &scope){
    if(!listed(id)||!QStringList{"install","update"}.contains(operation))return;
    for(auto value:m_jobs){auto job=value.toMap();if(job.value("app")==id&&QStringList{"queued","running"}.contains(job.value("state").toString())){m_status="This app already has a background operation.";emit changed();return;}}
    const auto start=[this,operation,id,scope]{worker({"start",operation,id,scope},[this](bool ok,const QVariantMap&){m_status=ok?"The app operation continues in the background. You may close Store.":"The app operation could not start. Check the app service and retry.";emit changed();pollJobs();});};
    if(m_catalog.value(id).value("sourceKind","flatpak")=="flatpak"&&operation=="install")ensureSource(start);else start();
}
void Store::updateApp(const QString &id){if(m_updatable.contains(id))startJob("update",id,m_updateScopes.value(id,"user"));}
void Store::cancelJob(const QString &id){if(!QRegularExpression("^[a-f0-9]{32}$").match(id).hasMatch())return;worker({"cancel",id},[this](bool,const QVariantMap&){pollJobs();});}
void Store::retryJob(const QString &id){for(auto value:m_jobs){auto job=value.toMap();if(job.value("job")==id&&QStringList{"failed","cancelled"}.contains(job.value("state").toString())){const auto app=job.value("app").toString();if(job.value("operation")=="update")startJob("update",app,job.value("scope","user").toString());else {select(app);review(app);}return;}}}
void Store::checkAppUpdates(){
    if(property("checkingUpdates").toBool())return;setProperty("checkingUpdates",true);m_status="Checking app updates…";emit changed();
    worker({"updates"},[this](bool ok,const QVariantMap &reply){
        setProperty("checkingUpdates",false);
        if(!ok){m_status="App updates could not be checked. Connect to the internet and retry.";emit changed();return;}
        m_updatable.clear();m_updateScopes.clear();
        for(auto value:reply.value("updates").toList()){auto app=value.toMap();const auto id=app.value("id").toString();m_updatable<<id;m_updateScopes[id]=app.value("scope").toString();}
        m_status=!reply.value("errors").toList().isEmpty()?"Some app sources could not be checked. Check your connection and retry.":m_updatable.isEmpty()?"Your Store apps are up to date.":QString::number(m_updatable.size())+" app updates are available.";
        emit jobsChanged();emit changed();
    });
}
