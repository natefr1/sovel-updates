#include "settingsservice.h"
#include "sovelmode.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTime>
#include <QColor>
#include <QPalette>
#include <QFont>
#include <QImageReader>
#include <QImage>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <cmath>
#include <memory>

SettingsService::SettingsService(QObject *parent,bool applyUserChanges):QObject(parent),m_applyUserChanges(applyUserChanges){
 QFile schema(":/assets/settings-pages.json");if(schema.open(QIODevice::ReadOnly)){auto root=QJsonDocument::fromJson(schema.readAll()).object();m_pages=root["pages"].toArray().toVariantList();m_definitions=root["preferences"].toObject().toVariantMap();}
 // Pages, items and preferences tagged with "modes" exist only in those modes.
 // Normal Mode never receives private page definitions, even through search.
 const auto allowed=[](const QVariantMap &entry){return SovelMode::allowedIn(entry.value("modes").toStringList());};
 QVariantList visible;
 for(const auto &value:m_pages){auto page=value.toMap();if(!allowed(page))continue;QVariantList sections;
  for(const auto &sectionValue:page.value("sections").toList()){auto section=sectionValue.toMap();if(!allowed(section))continue;QVariantList items;
   for(const auto &item:section.value("items").toList()){const auto entry=item.toMap();if(allowed(entry)&&(entry.value("type")!="preference"||allowed(m_definitions.value(entry.value("key").toString()).toMap())))items<<entry;}
   if(!items.isEmpty()){section["items"]=items;sections<<section;}}
  page["sections"]=sections;visible<<page;}
 m_pages=visible;
 m_file=QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/Sovel/Preferences.json";reload();
 m_poll.setInterval(2000);connect(&m_poll,&QTimer::timeout,this,[this]{reload();const int hour=QTime::currentTime().hour();if(hour!=m_themeHour){m_themeHour=hour;if(m_preferences.value("appearance.mode")=="automatic"){updateApplicationTheme();emit changed();if(m_applyUserChanges)applyPreferences();}}});m_poll.start();
}
SettingsService::~SettingsService(){for(auto p:m_jobs)if(p){p->disconnect(this);if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(500);}}}
bool SettingsService::valid(const QString &key,const QVariant &value)const{
 if(!m_definitions.contains(key))return false;const auto d=m_definitions[key].toMap();const auto kind=d["kind"].toString();
 if(kind=="switch")return value.metaType().id()==QMetaType::Bool;
 if(kind=="slider"){bool ok=false;const double n=value.toDouble(&ok);return ok&&std::isfinite(n)&&n>=d["minimum"].toDouble()&&n<=d["maximum"].toDouble()&&std::floor(n)==n;}
 if(kind=="choice")return d["choices"].toList().contains(value);
 if(kind=="color")return QRegularExpression("^#[0-9a-fA-F]{6}$").match(value.toString()).hasMatch();
 if(kind=="text"){auto text=value.toString();return !text.trimmed().isEmpty()&&text.size()<=d.value("maxLength",120).toInt()&&!text.contains(QRegularExpression("[\\x00-\\x1f\\x7f]"));}
 return false;
}
void SettingsService::reload(){
 QFile file(m_file);QVariantMap stored;if(file.size()<=256*1024&&file.open(QIODevice::ReadOnly))stored=QJsonDocument::fromJson(file.readAll()).object().toVariantMap();const bool avatarChanged=m_extra.value("avatar")!=stored.value("avatar");m_extra=stored;m_aliases.sync();QVariantMap result;
 for(auto it=m_definitions.begin();it!=m_definitions.end();++it){auto definition=it.value().toMap();auto value=stored.value(it.key(),definition["default"]);const auto alias=definition["alias"].toString();if(!alias.isEmpty())value=m_aliases.value(alias,definition["default"]);if(!valid(it.key(),value))value=definition["default"];result[it.key()]=value;}
 for(const auto &key:{QString("favorites"),QString("recent")}){QStringList ids;for(auto id:stored.value(key).toStringList())if(pageExists(id)&&!ids.contains(id))ids<<id;result[key]=ids.mid(0,key=="recent"?8:29);}
 if(result!=m_preferences){m_preferences=result;updateApplicationTheme();emit changed();}
 else if(avatarChanged)emit changed();
}
bool SettingsService::save(){
 auto stored=m_extra;for(auto it=m_preferences.begin();it!=m_preferences.end();++it)stored[it.key()]=it.value();const auto folder=QFileInfo(m_file).absolutePath();if(!QDir().mkpath(folder))return false;
 QSaveFile file(m_file);if(!file.open(QIODevice::WriteOnly))return false;file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument::fromVariant(stored).toJson(QJsonDocument::Indented));return file.commit();
}
bool SettingsService::setPreference(const QString &key,const QVariant &value){
 if(!valid(key,value)){m_status="That preference is not a supported value.";emit statusChanged();return false;}reload();const auto before=m_preferences;m_preferences[key]=value;
 const auto alias=m_definitions[key].toMap()["alias"].toString();if(!save()){m_preferences=before;m_status="Your preference could not be saved.";emit changed();emit statusChanged();return false;}
 if(!alias.isEmpty()){m_aliases.setValue(alias,value);m_aliases.sync();}
 m_status="Preference saved.";updateApplicationTheme();emit changed();emit statusChanged();
 if(m_applyUserChanges&&(key.startsWith("input.")||key.startsWith("desktop.")||key.startsWith("appearance.")||key.startsWith("notifications.")))applyPreferences();
 return true;
}
void SettingsService::applyPreferences(){
 if(m_jobs.contains("user/apply-preferences")){m_applyPending=true;return;}
 m_applyPending=false;run("apply-preferences",{},false,[this](bool ok,const QVariantMap &result){m_status=result.value(ok?"message":"error",ok?"Preference applied.":"The session did not apply this preference.").toString();emit statusChanged();if(m_applyPending)QTimer::singleShot(0,this,&SettingsService::applyPreferences);});
}
void SettingsService::updateApplicationTheme(){
 if(!m_applyUserChanges||!qobject_cast<QGuiApplication*>(QCoreApplication::instance()))return;
 const auto theme=colors();auto palette=QGuiApplication::palette();
 for(auto role:{QPalette::Window,QPalette::Base,QPalette::Button})palette.setColor(role,QColor(theme[role==QPalette::Base?"field":"background"].toString()));
 for(auto role:{QPalette::Text,QPalette::WindowText,QPalette::ButtonText})palette.setColor(role,QColor(theme["text"].toString()));
 palette.setColor(QPalette::Highlight,QColor(theme["accent"].toString()));palette.setColor(QPalette::HighlightedText,QColor(theme["accentText"].toString()));QGuiApplication::setPalette(palette);
 auto font=QGuiApplication::font();font.setFamily(m_preferences.value("appearance.font","Noto Sans").toString());font.setPixelSize(m_preferences.value("appearance.textSize",13).toInt());QGuiApplication::setFont(font);
}
QVariantMap SettingsService::colors()const{
 const bool isPrivate=SovelMode::isPrivate();
 // Private Mode is always the dark violet laboratory; Normal Mode is a quiet graphite desktop.
 const auto mode=isPrivate?QString("dark"):m_preferences.value("appearance.mode","dark").toString();const bool light=mode=="light"||(mode=="automatic"&&QTime::currentTime().hour()>=7&&QTime::currentTime().hour()<19);const bool contrast=m_preferences.value("accessibility.highContrast",false).toBool();
 const auto schemaAccent=m_definitions.value("appearance.accent").toMap().value("default").toString();
 auto chosen=m_preferences.value("appearance.accent",schemaAccent).toString();
 // The untouched schema default means "use this mode's own accent".
 if(chosen.compare(schemaAccent,Qt::CaseInsensitive)==0)chosen=isPrivate?"#BF40FA":"#7C9CFF";
 QColor accent(chosen);if(!accent.isValid())accent=QColor(isPrivate?"#BF40FA":"#7C9CFF");
 QVariantMap palette;
 if(isPrivate)palette={{"background","#07060B"},{"sidebar","#0E0A15"},{"surface","#151021"},{"surfaceHover","#24183A"},{"text","#F1EBFF"},{"muted","#B3A6CC"},{"border",contrast?"#E3D9FC":"#3A2A55"},{"field","#0B0812"},{"danger","#FF8FA8"},{"success","#7FE0B5"},{"secondary","#4928C2"},{"velvet","#5B2A62"},{"periwinkle","#E3D9FC"}};
 else palette={{"background",light?"#F4F5F7":"#121317"},{"sidebar",light?"#E9EBEF":"#191B21"},{"surface",light?"#FFFFFF":"#20232A"},{"surfaceHover",light?"#E2E6EE":"#2B2F38"},{"text",light?"#1B1D22":"#EEF0F4"},{"muted",light?"#5D6370":"#A7ADBA"},{"border",contrast?(light?"#3B4050":"#D8DCE6"):(light?"#D3D7DF":"#353944")},{"field",light?"#FFFFFF":"#16181D"},{"danger",light?"#B32745":"#FFA2B4"},{"success",light?"#25704D":"#9BD8B5"},{"secondary",light?"#4D6BD8":"#5B7BE8"},{"velvet",light?"#D9DDE6":"#2A2E38"},{"periwinkle",light?"#30343D":"#E6E9F0"}};
 palette["light"]=light;palette["private"]=isPrivate;palette["accent"]=accent.name();palette["accentText"]=accent.lightnessF()>0.62?"#14161B":"#FFFFFF";
 return palette;
}
bool SettingsService::pageExists(const QString &id)const{for(auto p:m_pages)if(p.toMap()["id"].toString()==id)return true;return false;}
QUrl SettingsService::avatarUrl()const{const auto folder=QFileInfo(m_file).absolutePath()+"/avatars/";const auto name=m_extra.value("avatar").toString();return name.startsWith(folder)&&QFileInfo(name).isFile()?QUrl::fromLocalFile(name):QUrl();}
bool SettingsService::setAvatar(const QUrl &url){
 if(!url.isLocalFile())return false;QImageReader reader(url.toLocalFile());reader.setAutoTransform(true);const auto size=reader.size();if(!reader.canRead()||!size.isValid()||qint64(size.width())*size.height()>32000000||QFileInfo(url.toLocalFile()).size()>64*1024*1024)return false;reader.setScaledSize(size.scaled(256,256,Qt::KeepAspectRatio));const auto image=reader.read();if(image.isNull())return false;
 const auto bytes=QByteArray(reinterpret_cast<const char*>(image.constBits()),image.sizeInBytes());const auto folder=QFileInfo(m_file).absolutePath()+"/avatars/";QDir().mkpath(folder);const auto name=folder+QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex())+".png";QSaveFile output(name);if(!output.open(QIODevice::WriteOnly))return false;output.setPermissions(QFile::ReadOwner|QFile::WriteOwner);if(!image.save(&output,"PNG")||!output.commit())return false;reload();m_extra["avatar"]=name;if(!save())return false;emit changed();return true;
}
void SettingsService::visit(const QString &id){if(!pageExists(id))return;reload();auto list=m_preferences["recent"].toStringList();list.removeAll(id);list.prepend(id);m_preferences["recent"]=list.mid(0,8);save();emit changed();}
void SettingsService::favorite(const QString &id,bool enabled){if(!pageExists(id))return;reload();auto list=m_preferences["favorites"].toStringList();list.removeAll(id);if(enabled)list<<id;m_preferences["favorites"]=list;save();emit changed();}
QString SettingsService::helper(bool admin)const{
 const auto name=admin?"sovel-settings-admin":"sovel-settings-helper";const auto installed=QString("/usr/libexec/")+name;if(QFileInfo(installed).isFile())return installed;
 const auto source=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../packaging/"+QString(name));return !admin&&QFileInfo(source).isFile()?source:QString();
}
void SettingsService::run(const QString &operation,const QVariantMap &payload,bool admin,std::function<void(bool,const QVariantMap&)> callback){
 const auto executable=helper(admin);const auto key=(admin?"admin/":"user/")+operation;
 if(m_jobs.contains(key)){m_status="That operation is already running.";emit statusChanged();return;}
 if(executable.isEmpty()){callback(false,{{"error","The settings service is unavailable. Install this Sovel update before applying system changes."}});return;}
 auto p=new QProcess(this);m_jobs[key]=p;emit statusChanged();auto completed=std::make_shared<bool>(false);auto timer=new QTimer(p);timer->setSingleShot(true);
 connect(p,&QProcess::started,p,[p,payload,timer,admin]{p->write(QJsonDocument::fromVariant(payload).toJson(QJsonDocument::Compact));p->closeWriteChannel();timer->start(admin?120000:20000);});
 connect(timer,&QTimer::timeout,p,[p]{p->kill();});
 connect(p,&QProcess::readyReadStandardOutput,p,[p]{if(p->bytesAvailable()>1024*1024)p->kill();});
 auto finish=[this,p,key,completed,callback](bool ok){if(*completed)return;*completed=true;m_jobs.remove(key);auto result=QJsonDocument::fromJson(p->readAllStandardOutput()).object().toVariantMap();if(result.isEmpty())result={{"error",QString("The settings operation did not complete%1.").arg(p->exitCode()==126||p->exitCode()==127?" because authorization was not granted":"")}};ok=ok&&!result.contains("error");callback(ok,result);p->deleteLater();emit statusChanged();};
 connect(p,&QProcess::errorOccurred,this,[finish](QProcess::ProcessError){finish(false);});connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[finish](int code,QProcess::ExitStatus status){finish(code==0&&status==QProcess::NormalExit);});
 if(admin)p->start("pkexec",{executable,operation});else p->start("python3",{"-I",executable,operation});
}
void SettingsService::refresh(const QString &page,bool force){
 if(!pageExists(page)||m_jobs.contains("user/report-"+page))return;const auto now=QDateTime::currentMSecsSinceEpoch();if(!force&&m_refreshed.value(page)>now-30000)return;
 run("report-"+page,{},false,[this,page](bool ok,const QVariantMap &result){auto report=result;report["available"]=ok;report["checkedAt"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);m_reports[page]=report;if(ok)m_refreshed[page]=QDateTime::currentMSecsSinceEpoch();emit reportsChanged();});
}
void SettingsService::perform(const QString &operation,const QVariantMap &payload){
 static const QSet<QString> admin={"set-hostname","set-timezone","set-ntp","set-full-name","firewall-status","set-firewall"};
 static const QSet<QString> user={"display-config","identify-displays","input-config","printers","scanner","network-config","localsend","kdeconnect","notification-history","clear-notifications","default-apps","set-default-app","startup-items","set-startup","disks","disk-usage","trash","account-security","sessions","choose-locale","set-locale","fonts","screen-reader","on-screen-keyboard","resources","steam","heroic","bottles","screenshot","flatseal","backup-tool","logs","licenses","terminal","apply-preferences","sleep-inhibitors","ssh-review","isolation-check","private-audit","private-setup","tablet-config"};
 if(!admin.contains(operation)&&!user.contains(operation)){m_status="This action is not available in the settings service.";emit statusChanged();emit actionFinished(operation,false,{{"error",m_status}});return;}
 run(operation,payload,admin.contains(operation),[this,operation](bool ok,const QVariantMap &result){m_status=result.value(ok?"message":"error",ok?"Operation completed.":"The change was not applied.").toString();emit statusChanged();emit actionFinished(operation,ok,result);if(ok)m_refreshed.clear();});
}
QVariantMap SettingsService::readProfile(const QUrl &url){
 if(!url.isLocalFile())return {{"error","Choose a local profile file."}};QFile file(url.toLocalFile());if(file.size()>128*1024||!file.open(QIODevice::ReadOnly))return {{"error","The profile could not be read."}};
 auto object=QJsonDocument::fromJson(file.readAll()).object();if(object["format"].toString()!="sovel-appearance-1")return {{"error","This is not a Sovel appearance profile."}};auto prefs=object["preferences"].toObject().toVariantMap();
 for(auto it=prefs.begin();it!=prefs.end();++it)if((!it.key().startsWith("appearance.")&&it.key()!="reduceMotion"&&it.key()!="reduceTransparency")||!valid(it.key(),it.value()))return {{"error","The profile contains an unsupported preference."}};
 return {{"preferences",prefs},{"count",prefs.size()}};
}
bool SettingsService::applyProfile(const QVariantMap &profile){auto prefs=profile.value("preferences").toMap();if(prefs.isEmpty())return false;for(auto it=prefs.begin();it!=prefs.end();++it)if((!it.key().startsWith("appearance.")&&it.key()!="reduceMotion"&&it.key()!="reduceTransparency")||!valid(it.key(),it.value()))return false;for(auto it=prefs.begin();it!=prefs.end();++it)if(!setPreference(it.key(),it.value()))return false;return true;}
bool SettingsService::exportProfile(const QUrl &url){if(!url.isLocalFile())return false;QVariantMap prefs;for(auto it=m_preferences.begin();it!=m_preferences.end();++it)if(it.key().startsWith("appearance.")||it.key()=="reduceMotion"||it.key()=="reduceTransparency")prefs[it.key()]=it.value();QSaveFile file(url.toLocalFile());if(!file.open(QIODevice::WriteOnly))return false;file.write(QJsonDocument::fromVariant(QVariantMap{{"format","sovel-appearance-1"},{"preferences",prefs}}).toJson());return file.commit();}
bool SettingsService::exportDiagnostics(const QUrl &url){if(!m_preferences.value("privacy.diagnostics").toBool()||!url.isLocalFile())return false;QSaveFile file(url.toLocalFile());if(!file.open(QIODevice::WriteOnly))return false;QVariantMap summary;for(const auto &id:{QString("about"),QString("displays"),QString("power"),QString("developer")})if(m_reports.contains(id))summary[id]=m_reports[id];file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument::fromVariant(QVariantMap{{"format","sovel-diagnostics-1"},{"createdAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"reports",summary}}).toJson());return file.commit();}
void SettingsService::resetAppearance(){for(auto it=m_definitions.begin();it!=m_definitions.end();++it)if(it.key().startsWith("appearance.")||it.key()=="reduceMotion"||it.key()=="reduceTransparency")setPreference(it.key(),it.value().toMap()["default"]);}
