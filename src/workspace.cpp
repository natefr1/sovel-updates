#include "workspace.h"
#include "backend.h"
#include "settingsservice.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QLockFile>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimeZone>
#include <QRegularExpression>
#include <QUuid>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <cmath>
#include <algorithm>
#include <memory>
#include <functional>
#include <QDirIterator>
#include <QLocalSocket>
#include <QCoreApplication>
namespace {
constexpr int MaxWidgetOutputs=16;
bool validOutput(const QString &output)
{
 return !output.isEmpty()&&output.size()<=120&&!output.contains(QChar(0));
}
QVariantMap widgetGeometry(const QVariantMap &input)
{
 bool validX=false,validY=false,validWidth=false,validHeight=false;
 const auto x=input.value("x").toDouble(&validX),y=input.value("y").toDouble(&validY);
 const auto width=input.value("width").toInt(&validWidth),height=input.value("height").toInt(&validHeight);
 if(!validX||!validY||!validWidth||!validHeight||!std::isfinite(x)||!std::isfinite(y))return {};
 if((width!=0||height!=0)&&(width<176||height<150||width>4096||height>2160))return {};
 return {{"x",qBound(0.,x,1.)},{"y",qBound(0.,y,1.)},{"width",width},{"height",height}};
}
QVariantMap jsonFile(const QString &name){QFile f(name);if(f.size()>1024*1024||!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object().toVariantMap();}
QString read(const QString &name){QFile f(name);return f.open(QIODevice::ReadOnly)?QString::fromUtf8(f.readAll()).trimmed():QString();}
void dbus(QObject *owner,const QString &service,const QString &path,const QString &interface,const QString &method,const QVariantList &args,std::function<void(const QDBusMessage&)> done){auto m=QDBusMessage::createMethodCall(service,path,interface,method);m.setArguments(args);auto w=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(m,3000),owner);QObject::connect(w,&QDBusPendingCallWatcher::finished,owner,[w,done]{done(w->reply());w->deleteLater();});}
}
Workspace::Workspace(Backend *backend,SettingsService *settings,bool desktop,QObject *parent):QObject(parent),m_backend(backend),m_settings(settings),m_desktop(desktop),m_network(this){
 QFile catalog(":/assets/widget-catalog.json");if(catalog.open(QIODevice::ReadOnly))m_registry=QJsonDocument::fromJson(catalog.readAll()).array().toVariantList();
 m_file=QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/Sovel/Workspace.json";reload();
 if(!QFile::exists(m_file)){
  auto legacy=backend->widgets();if(legacy.isEmpty())legacy={QVariantMap{{"type","clock"},{"x",.98},{"y",.12}},QVariantMap{{"type","jared"},{"x",.98},{"y",.40}},QVariantMap{{"type","system"},{"x",.98},{"y",.74}}};
  m_extra["legacyLayout"]=legacy;m_dirty.insert("legacyLayout");
  for(int i=0;i<legacy.size();i++){auto widget=legacy[i].toMap();widget["size"]=widget.value("type")=="system"?"medium":"small";widget["x"]=i%2?.02:.98;widget["y"]=i<2?.10:.68;legacy[i]=widget;}
  saveLayout(legacy);
 }
 m_tickClock.start();m_poll.setInterval(2000);connect(&m_poll,&QTimer::timeout,this,&Workspace::tick);m_poll.start();
 connect(backend,&Backend::metricsChanged,this,[this]{if(m_desktop)sampleTelemetry();});
 connect(settings,&SettingsService::changed,this,[this]{if(!networkAllowed()&&(m_weatherBusy||m_weather.value("available").toBool()||!m_cities.isEmpty()||!m_network.findChildren<QNetworkReply*>().isEmpty())){++m_weatherGeneration;m_weatherBusy=false;for(auto r:m_network.findChildren<QNetworkReply*>())r->abort();m_cities.clear();m_weather["status"]="Weather access is off.";m_weather["available"]=false;m_dirty.insert("weather");save();emit weatherChanged();}});
 if(m_desktop){
  sampleTelemetry();
  const auto directory=QFileInfo(assistantStatePath()).absolutePath();
  if(QDir().mkpath(directory)){
   m_assistantWatcher.addPath(directory);
   connect(&m_assistantWatcher,&QFileSystemWatcher::directoryChanged,this,&Workspace::readAssistantState);
   readAssistantState();
  }
 }
}
QString Workspace::assistantStatePath()
{
 return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"/sovel-assistant/state.json";
}
void Workspace::readAssistantState()
{
 auto state=jsonFile(assistantStatePath());
#ifdef Q_OS_LINUX
 const auto pid=state.value("pid").toLongLong();
 if(pid<=0||!QFileInfo::exists(QString("/proc/%1").arg(pid)))state.clear();
#endif
 if(state!=m_assistant){m_assistant=state;emit assistantChanged();}
}
void Workspace::assistantCommand(const QString &action,const QString &text)
{
 static const QStringList allowed{"connect","ask","voice-start","voice-stop","clear-transcript","approve","reject","choose-output","cancel","show","preferences","unload","load-workspaces","project-summary","project-task","project-open-file"};
 if(!m_desktop||!allowed.contains(action)||text.size()>2000)return;
 const auto endpoint=QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)+"/sovel-jared.sock";
 auto *socket=new QLocalSocket(this);
 const auto request=QJsonDocument(QJsonObject{{"action",action},{"text",text}}).toJson(QJsonDocument::Compact)+"\n";
 connect(socket,&QLocalSocket::connected,socket,[socket,request]{socket->write(request);socket->disconnectFromServer();});
 connect(socket,&QLocalSocket::disconnected,socket,&QObject::deleteLater);
 connect(socket,&QLocalSocket::errorOccurred,this,[this,socket,endpoint,action,attempt=0](QLocalSocket::LocalSocketError)mutable{
  if(attempt==0&&action!="voice-stop"&&action!="clear-transcript")
   QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--jared-service"});
  if(++attempt>=24||action=="voice-stop"||action=="clear-transcript"){
   socket->deleteLater();
   readAssistantState();
   if(attempt>=24&&m_assistant.isEmpty()){
    m_assistant={{"state","offline"},{"status","JARED did not start. Open the panel again to retry."}};
    emit assistantChanged();
   }
   return;
  }
  QTimer::singleShot(125,socket,[socket,endpoint]{socket->connectToServer(endpoint);});
 });
 socket->connectToServer(endpoint);
}
Workspace::~Workspace(){for(auto r:m_network.findChildren<QNetworkReply*>()){r->disconnect(this);r->abort();}if(m_gpu){m_gpu->disconnect(this);m_gpu->kill();m_gpu->waitForFinished(300);}}
bool Workspace::knownType(const QString &type)const{for(auto w:m_registry)if(w.toMap()["type"]==type)return true;return false;}
bool Workspace::hasWidget(const QString &type)const{for(auto w:m_layout)if(w.toMap()["type"]==type)return true;return false;}
void Workspace::reload(){
 auto values=jsonFile(m_file);if(!m_dirty.contains("usageReset")&&values.value("usageReset")!=m_extra.value("usageReset"))m_dirty.remove("usage");for(auto key:m_dirty)if(m_extra.contains(key))values[key]=m_extra[key];
 if(!m_dirty.contains("layout")){auto layout=values.value("layout").toList();if(layout!=m_layout){m_layout=layout;emit layoutChanged();}}
 if(!m_dirty.contains("data")){auto data=values.value("data").toMap();if(data!=m_data){m_data=data;emit dataChanged();}}
 if(!m_dirty.contains("usage")){auto usage=values.value("usage").toMap();if(usage!=m_usage){m_usage=usage;emit usageChanged();}}
 if(!m_dirty.contains("weather")){auto weather=values.value("weather").toMap();if(weather!=m_weather){m_weather=weather;emit weatherChanged();}}
 m_extra=values;
}
bool Workspace::save(){
 QDir().mkpath(QFileInfo(m_file).absolutePath());QLockFile lock(m_file+".lock");if(!lock.tryLock(100)){m_status="Workspace is busy. Your change will be retried.";emit statusChanged();return false;}
 auto values=jsonFile(m_file);for(auto key:m_dirty)values[key]=key=="layout"?QVariant(m_layout):key=="data"?QVariant(m_data):key=="usage"?QVariant(m_usage):key=="weather"?QVariant(m_weather):m_extra.value(key);
 QSaveFile file(m_file);if(!file.open(QIODevice::WriteOnly)){m_status="Workspace preferences could not be saved.";emit statusChanged();return false;}file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);auto bytes=QJsonDocument::fromVariant(values).toJson(QJsonDocument::Compact);if(file.write(bytes)!=bytes.size()||!file.commit())return false;
 m_dirty.clear();m_extra=values;m_lastSave=m_tickClock.isValid()?m_tickClock.elapsed():0;return true;
}
bool Workspace::saveLayout(const QVariantList &list){
 if(list.size()>32)return false;QVariantList clean;QSet<QString> ids;
 for(auto value:list){auto w=value.toMap();const auto type=w.value("type").toString();if(!knownType(type))return false;auto id=w.value("id").toString();if(id.isEmpty())id=QUuid::createUuid().toString(QUuid::WithoutBraces);if(ids.contains(id)||!QRegularExpression("^[A-Za-z0-9-]{1,80}$").match(id).hasMatch())return false;ids.insert(id);
  const auto x=w.value("x",.1).toDouble(),y=w.value("y",.1).toDouble();if(!std::isfinite(x)||!std::isfinite(y))return false;auto size=w.value("size","medium").toString();if(!QStringList{"small","medium","large"}.contains(size))return false;
  QVariantMap placements;
  const auto saved=w.value("placements").toMap();
  if(saved.size()>MaxWidgetOutputs)return false;
  for(auto it=saved.cbegin();it!=saved.cend();++it){
   const auto geometry=widgetGeometry(it.value().toMap());
   if(!validOutput(it.key())||geometry.isEmpty())return false;
   placements.insert(it.key(),geometry);
  }
  clean<<QVariantMap{{"id",id},{"type",type},{"x",qBound(0.,x,1.)},{"y",qBound(0.,y,1.)},{"size",size},{"opacity",qBound(25,w.value("opacity",75).toInt(),100)},{"refresh",qBound(1,w.value("refresh",type=="weather"?900:2).toInt(),3600)},{"placements",placements}};
 }
 reload();m_layout=clean;m_dirty.insert("layout");const bool ok=save();emit layoutChanged();return ok;
}
QString Workspace::addWidget(const QString &type){if(!knownType(type))return {};reload();const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);auto list=m_layout;list<<QVariantMap{{"id",id},{"type",type},{"x",.1+(.15*(list.size()%5))},{"y",.12+(.13*(list.size()%4))}};return saveLayout(list)?id:QString();}
void Workspace::removeWidget(const QString &id){reload();auto list=m_layout;for(int i=0;i<list.size();i++)if(list[i].toMap()["id"]==id){list.removeAt(i);saveLayout(list);m_data.remove(id);m_dirty.insert("data");save();emit dataChanged();return;}}
bool Workspace::moveWidget(const QString &id,double x,double y){if(!std::isfinite(x)||!std::isfinite(y))return false;reload();auto list=m_layout;for(int i=0;i<list.size();i++){auto w=list[i].toMap();if(w["id"]==id){w["x"]=qBound(0.,x,1.);w["y"]=qBound(0.,y,1.);list[i]=w;return saveLayout(list);}}return false;}
bool Workspace::placeWidget(const QString &id,const QString &output,double x,double y,int width,int height)
{
 const auto geometry=widgetGeometry({{"x",x},{"y",y},{"width",width},{"height",height}});
 if(!validOutput(output)||geometry.isEmpty())return false;
 reload();
 auto layout=m_layout;
 for(auto &entry:layout){
  auto widget=entry.toMap();
  if(widget.value("id")!=id)continue;
  auto placements=widget.value("placements").toMap();
  const auto previous=placements.value(output).toMap();
  if(previous==geometry)return true;
  placements.insert(output,geometry);
  widget["placements"]=placements;
  entry=widget;
  if(!saveLayout(layout))return false;
  auto &history=m_placementHistory[output];
  history.append({id,previous});
  if(history.size()>16)history.removeFirst();
  return true;
 }
 return false;
}
bool Workspace::undoWidgetPlacement(const QString &output)
{
 auto history=m_placementHistory.find(output);
 if(history==m_placementHistory.end())return false;
 reload();
 while(!history->isEmpty()){
  const auto change=history->takeLast();
  auto layout=m_layout;
  for(auto &entry:layout){
   auto widget=entry.toMap();
   if(widget.value("id")!=change.id)continue;
   auto placements=widget.value("placements").toMap();
   if(change.previous.isEmpty())placements.remove(output);
   else placements.insert(output,change.previous);
   widget["placements"]=placements;
   entry=widget;
   if(saveLayout(layout))return true;
   history->append(change);
   return false;
  }
 }
 return false;
}
bool Workspace::configureWidget(const QString &id,const QString &size,int opacity,int refresh){reload();auto list=m_layout;for(int i=0;i<list.size();i++){auto w=list[i].toMap();if(w["id"]!=id)continue;w["size"]=size;w["opacity"]=opacity;w["refresh"]=refresh;list[i]=w;return saveLayout(list);}return false;}
bool Workspace::setWidgetData(const QString &id,const QVariantMap &data){reload();bool exists=false;for(auto w:m_layout)exists|=w.toMap()["id"]==id;if(!exists||QJsonDocument::fromVariant(data).toJson().size()>8192)return false;m_data[id]=data;m_dirty.insert("data");const bool ok=save();emit dataChanged();return ok;}
void Workspace::setForeground(const QString &appId){m_foreground=QRegularExpression("^[A-Za-z0-9_.:-]{1,200}$").match(appId).hasMatch()?appId:QString();}
QVariantList Workspace::usage()const{QVariantList result;for(auto day=m_usage.begin();day!=m_usage.end();++day){const auto apps=day.value().toMap();for(auto app=apps.begin();app!=apps.end();++app)result<<QVariantMap{{"day",day.key()},{"app",app.key()},{"seconds",app.value()}};}return result;}
void Workspace::clearUsage(){reload();m_usage.clear();m_extra["usageReset"]=QUuid::createUuid().toString();m_dirty.insert("usageReset");m_dirty.insert("usage");save();m_breakElapsed=0;emit usageChanged();}
void Workspace::tick(){
 const auto now=m_tickClock.elapsed();const qint64 delta=qBound(qint64(0),now-m_lastTick,qint64(5000));m_lastTick=now;reload();
 if(m_desktop){
#ifdef Q_OS_LINUX
  const auto assistantPid=m_assistant.value("pid").toLongLong();
  if(assistantPid>0&&!QFileInfo::exists(QString("/proc/%1").arg(assistantPid)))readAssistantState();
#endif
  const auto wallClock=QDateTime::currentMSecsSinceEpoch();bool changed=false;
  for(auto value:m_layout){const auto widget=value.toMap();const auto id=widget.value("id").toString();auto content=m_data.value(id).toMap();const auto type=widget.value("type").toString();
   if(type=="timer"&&content.value("deadline").toLongLong()>0&&content.value("deadline").toLongLong()<=wallClock&&!content.value("notified").toBool()){notify("Timer finished","Your desktop timer has finished.");content["notified"]=true;content["deadline"]=0;content["seconds"]=0;m_data[id]=content;changed=true;}
   if(type=="reminders"||type=="events"){auto items=content.value("items").toList();for(int i=0;i<items.size();i++){auto item=items[i].toMap();const auto due=QDateTime::fromString(item.value("due").toString(),Qt::ISODate);if(due.isValid()&&due.toMSecsSinceEpoch()<=wallClock&&!item.value("done").toBool()&&!item.value("notified").toBool()){notify(type=="events"?"Upcoming event":"Reminder",item.value("text").toString());item["notified"]=true;items[i]=item;changed=true;}}content["items"]=items;m_data[id]=content;}
  }
  if(changed){m_dirty.insert("data");save();emit dataChanged();}
 }
 const auto p=m_settings->preferences();if(m_desktop&&p.value("usage.enabled").toBool()&&!m_foreground.isEmpty()){
  const auto today=QDate::currentDate();auto day=m_usage.value(today.toString(Qt::ISODate)).toMap();day[m_foreground]=day.value(m_foreground).toDouble()+delta/1000.;m_usage[today.toString(Qt::ISODate)]=day;
  for(auto key:m_usage.keys())if(QDate::fromString(key,Qt::ISODate)<today.addDays(-p.value("usage.retention",7).toInt()+1))m_usage.remove(key);m_dirty.insert("usage");emit usageChanged();
  if(p.value("usage.breakReminders").toBool()){m_breakElapsed+=delta/1000.;if(m_breakElapsed>=p.value("usage.breakMinutes",60).toInt()*60){notify("Time for a break","A short pause can help. Your local break reminder interval has elapsed.");m_breakElapsed=0;}}
 }
 if(m_desktop){const auto cutoff=QDate::currentDate().addDays(-p.value("usage.retention",7).toInt()+1);bool pruned=false;for(auto day:m_usage.keys())if(QDate::fromString(day,Qt::ISODate)<cutoff){m_usage.remove(day);pruned=true;}if(pruned){m_dirty.insert("usage");emit usageChanged();}}
 if(!m_dirty.isEmpty()&&now-m_lastSave>30000)save();
 if(!m_desktop||!m_active||!p.value("widgets.visible",true).toBool())return;
 auto interval=[this](const QStringList &types,int minimum,int fallback){int value=3600;bool found=false;for(auto w:m_layout){const auto widget=w.toMap();if(types.contains(widget["type"].toString())){found=true;value=qMin(value,widget.value("refresh",fallback).toInt());}}return found?qBound(minimum,value,3600):fallback;};
 if(now-m_lastGpu>interval({"gpu","system","ai-monitor"},10,10)*1000&&(hasWidget("gpu")||hasWidget("system")||hasWidget("ai-monitor"))){m_lastGpu=now;sampleGpu();}
 if(now-m_lastMedia>interval({"media"},3,3)*1000&&hasWidget("media")){m_lastMedia=now;sampleMedia();}
 if(hasWidget("weather")&&QDateTime::currentSecsSinceEpoch()-m_lastWeather>interval({"weather"},300,900))refreshWeather();
}
void Workspace::sampleTelemetry(){
 auto result=m_backend->metrics();const auto now=QDateTime::currentMSecsSinceEpoch();quint64 rx=0,tx=0;QVariantList interfaces;
 for(auto line:read("/proc/net/dev").split('\n')){const auto colon=line.indexOf(':');if(colon<0)continue;const auto name=line.left(colon).trimmed();if(name=="lo")continue;const auto fields=line.mid(colon+1).simplified().split(' ');if(fields.size()<16)continue;rx+=fields[0].toULongLong();tx+=fields[8].toULongLong();const auto base="/sys/class/net/"+name;interfaces<<QVariantMap{{"name",name},{"connected",read(base+"/operstate")=="up"},{"wireless",QFileInfo::exists(base+"/wireless")||QFileInfo::exists(base+"/phy80211")}};}
 result["interfaces"]=interfaces;result["rxPerSecond"]=m_networkSample&&now>m_networkSample&&rx>=m_rx?double(rx-m_rx)*1000/(now-m_networkSample):0.;result["txPerSecond"]=m_networkSample&&now>m_networkSample&&tx>=m_tx?double(tx-m_tx)*1000/(now-m_networkSample):0.;m_rx=rx;m_tx=tx;m_networkSample=now;
 if(hasWidget("downloads")&&m_active&&now-m_lastDownloads>10000){m_lastDownloads=now;QList<QPair<qint64,QString>> files;QDirIterator iterator(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),QDir::Files|QDir::NoSymLinks);int examined=0;while(iterator.hasNext()&&examined++<500){iterator.next();const auto info=iterator.fileInfo();files.append({info.lastModified().toMSecsSinceEpoch(),info.fileName()});}std::sort(files.begin(),files.end(),[](const auto&a,const auto&b){return a.first>b.first;});QStringList names;for(int i=0;i<files.size()&&i<5;i++)names<<files[i].second;m_downloads=names;}result["downloads"]=m_downloads;
 QVariantList batteries;for(auto name:QDir("/sys/class/power_supply").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){const auto base="/sys/class/power_supply/"+name;const auto type=read(base+"/type");if(type=="Battery"||type=="UPS"){bool ok=false;auto capacity=read(base+"/capacity").toInt(&ok);batteries<<QVariantMap{{"name",name},{"percent",ok?qBound(0,capacity,100):-1},{"status",read(base+"/status")}};}}
 result["batteries"]=batteries;for(auto key:{"gpu","gpuName","gpuAvailable","gpuMemoryUsed","gpuMemoryTotal"})if(m_telemetry.contains(key))result[key]=m_telemetry[key];m_telemetry=result;emit telemetryChanged();
}
void Workspace::sampleGpu(){
 for(auto name:QDir("/sys/class/drm").entryList({"card[0-9]*"},QDir::Dirs)){bool ok=false;const auto value=read("/sys/class/drm/"+name+"/device/gpu_busy_percent").toInt(&ok);if(ok){m_telemetry["gpu"]=qBound(0,value,100);m_telemetry["gpuAvailable"]=true;emit telemetryChanged();return;}}
 if(m_gpuBusy||QStandardPaths::findExecutable("nvidia-smi").isEmpty())return;m_gpuBusy=true;auto p=new QProcess(this);m_gpu=p;auto timer=new QTimer(p);timer->setSingleShot(true);connect(timer,&QTimer::timeout,p,&QProcess::kill);timer->start(2000);
 auto complete=std::make_shared<bool>(false);auto done=[this,p,complete](bool ok){if(*complete)return;*complete=true;m_gpuBusy=false;const auto fields=QString::fromUtf8(p->readAllStandardOutput()).split('\n').first().split(',');bool numeric=false;const auto value=fields.value(0).trimmed().toDouble(&numeric);m_telemetry["gpuAvailable"]=ok&&numeric;if(ok&&numeric){m_telemetry["gpu"]=qBound(0.,value,100.);m_telemetry["gpuMemoryUsed"]=fields.value(1).trimmed().toDouble();m_telemetry["gpuMemoryTotal"]=fields.value(2).trimmed().toDouble();m_telemetry["gpuName"]=fields.value(3).trimmed();}emit telemetryChanged();p->deleteLater();};
 connect(p,&QProcess::errorOccurred,this,[done](QProcess::ProcessError){done(false);});connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[done](int code,QProcess::ExitStatus status){done(code==0&&status==QProcess::NormalExit);});p->start("nvidia-smi",{"--query-gpu=utilization.gpu,memory.used,memory.total,name","--format=csv,noheader,nounits"});
}
bool Workspace::networkAllowed()const{return m_settings->preferences().value("weather.enabled",false).toBool();}
void Workspace::searchCity(const QString &query){
 if(!networkAllowed()){m_status="Enable weather access in Settings before searching for a city.";emit statusChanged();return;}if(query.trimmed().size()<2||query.size()>100)return;
 QUrl url("https://geocoding-api.open-meteo.com/v1/search");QUrlQuery q;q.addQueryItem("name",query.trimmed());q.addQueryItem("count","10");q.addQueryItem("language","en");url.setQuery(q);QNetworkRequest request(url);request.setTransferTimeout(15000);auto reply=m_network.get(request);connect(reply,&QNetworkReply::readyRead,this,[reply]{if(reply->bytesAvailable()>1024*1024)reply->abort();});
 connect(reply,&QNetworkReply::finished,this,[this,reply]{m_cities.clear();if(networkAllowed()&&reply->error()==QNetworkReply::NoError){for(auto value:QJsonDocument::fromJson(reply->readAll()).object()["results"].toArray()){auto c=value.toObject();if(!c["latitude"].isDouble()||!c["longitude"].isDouble())continue;m_cities<<QVariantMap{{"name",c["name"].toString()},{"region",c["admin1"].toString()},{"country",c["country"].toString()},{"latitude",c["latitude"].toDouble()},{"longitude",c["longitude"].toDouble()}};}}m_status=m_cities.isEmpty()?"No city results. Check your connection or try another name.":"Choose the matching city. No automatic location was requested.";emit statusChanged();emit weatherChanged();reply->deleteLater();});
}
void Workspace::chooseCity(int index){if(!networkAllowed()||index<0||index>=m_cities.size())return;reload();m_extra["city"]=m_cities[index];m_dirty.insert("city");m_weather.clear();m_dirty.insert("weather");save();++m_weatherGeneration;m_weatherBusy=false;m_lastWeather=0;refreshWeather();}
void Workspace::refreshWeather(){
 if(!networkAllowed()||m_weatherBusy)return;reload();const auto city=m_extra.value("city").toMap();if(!city.contains("latitude")||!city.contains("longitude")){m_weather={{"available",false},{"status","Choose your city in Settings."}};emit weatherChanged();return;}
 const auto latitude=city.value("latitude").toDouble(),longitude=city.value("longitude").toDouble();if(!std::isfinite(latitude)||!std::isfinite(longitude)||qAbs(latitude)>90||qAbs(longitude)>180)return;
 m_lastWeather=QDateTime::currentSecsSinceEpoch();m_weatherBusy=true;const auto generation=++m_weatherGeneration;QUrl url("https://api.open-meteo.com/v1/forecast");QUrlQuery q;for(auto pair:QList<QPair<QString,QString>>{{"latitude",QString::number(latitude)},{"longitude",QString::number(longitude)},{"current","temperature_2m,weather_code"},{"daily","temperature_2m_max,temperature_2m_min,weather_code"},{"forecast_days","5"},{"timezone","auto"}})q.addQueryItem(pair.first,pair.second);url.setQuery(q);QNetworkRequest request(url);request.setTransferTimeout(15000);auto reply=m_network.get(request);connect(reply,&QNetworkReply::readyRead,this,[reply]{if(reply->bytesAvailable()>1024*1024)reply->abort();});
 connect(reply,&QNetworkReply::finished,this,[this,reply,city,generation]{
  if(generation!=m_weatherGeneration||!networkAllowed()){reply->deleteLater();return;}m_weatherBusy=false;reload();if(m_extra.value("city").toMap()!=city){reply->deleteLater();return;}
  const auto object=QJsonDocument::fromJson(reply->readAll()).object(),current=object["current"].toObject(),daily=object["daily"].toObject();
  if(reply->error()==QNetworkReply::NoError&&current["temperature_2m"].isDouble()){
   QVariantList days;auto dates=daily["time"].toArray(),high=daily["temperature_2m_max"].toArray(),low=daily["temperature_2m_min"].toArray(),codes=daily["weather_code"].toArray();for(int i=0;i<dates.size()&&i<high.size()&&i<low.size()&&i<codes.size();i++)if(high.at(i).isDouble()&&low.at(i).isDouble())days<<QVariantMap{{"date",dates.at(i).toString()},{"high",high.at(i).toDouble()},{"low",low.at(i).toDouble()},{"code",codes.at(i).toInt()}};
   m_weather={{"available",true},{"city",city["name"]},{"temperature",current["temperature_2m"].toDouble()},{"code",current["weather_code"].toInt()},{"forecast",days},{"updatedAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"status","Weather by Open-Meteo · location data by GeoNames"}};
  }else {m_weather["status"]="Weather could not refresh. Showing cached data when available.";}
  m_dirty.insert("weather");save();emit weatherChanged();reply->deleteLater();
 });
}
void Workspace::sampleMedia(){
 if(m_mediaBusy)return;m_mediaBusy=true;dbus(this,"org.freedesktop.DBus","/org/freedesktop/DBus","org.freedesktop.DBus","ListNames",{},[this](const QDBusMessage &reply){
  struct Round{int pending=1,score=0;QVariantMap selected;QString service;};auto round=std::make_shared<Round>();auto done=[this,round]{if(--round->pending)return;m_mediaBusy=false;m_mediaService=round->service;m_media=round->selected;emit mediaChanged();};
  for(auto name:reply.arguments().value(0).toStringList())if(name.startsWith("org.mpris.MediaPlayer2.")){++round->pending;dbus(this,name,"/org/mpris/MediaPlayer2","org.freedesktop.DBus.Properties","GetAll",{"org.mpris.MediaPlayer2.Player"},[round,done,name](const QDBusMessage &r){auto p=qdbus_cast<QVariantMap>(r.arguments().value(0));const int score=p.value("PlaybackStatus")=="Playing"?2:p.isEmpty()?0:1;if(score>round->score){round->score=score;auto meta=qdbus_cast<QVariantMap>(p.value("Metadata"));round->service=name;round->selected={{"available",true},{"title",meta.value("xesam:title")},{"artist",meta.value("xesam:artist").toStringList().join(", ")},{"playing",p.value("PlaybackStatus")=="Playing"},{"canControl",p.value("CanControl")},{"canNext",p.value("CanGoNext")},{"canPrevious",p.value("CanGoPrevious")},{"canPlay",p.value("CanPlay")},{"canPause",p.value("CanPause")}};}done();});}done();
 });
}
void Workspace::mediaAction(const QString &action){if(m_mediaService.isEmpty()||!m_media.value("canControl").toBool()||!QStringList{"PlayPause","Next","Previous"}.contains(action))return;dbus(this,m_mediaService,"/org/mpris/MediaPlayer2","org.mpris.MediaPlayer2.Player",action,{},[this](const QDBusMessage &){sampleMedia();});}
QString Workspace::calculate(const QString &text)const{const auto m=QRegularExpression("^\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*([+*/-])\\s*(-?[0-9]+(?:\\.[0-9]+)?)\\s*$").match(text);if(!m.hasMatch())return "Use two numbers with +, −, × or ÷.";const double a=m.captured(1).toDouble(),b=m.captured(3).toDouble();const auto op=m.captured(2);if(op=="/"&&b==0)return "Cannot divide by zero.";const double answer=op=="+"?a+b:op=="-"?a-b:op=="*"?a*b:a/b;return std::isfinite(answer)?QString::number(answer,'g',12):QString("Result is too large.");}
QString Workspace::timeInZone(const QString &zone)const{QTimeZone timeZone(zone.toUtf8());return timeZone.isValid()?QDateTime::currentDateTimeUtc().toTimeZone(timeZone).toString("ddd h:mm AP"):QString("Choose a valid IANA time zone.");}
void Workspace::notify(const QString &title,const QString &message){dbus(this,"org.freedesktop.Notifications","/org/freedesktop/Notifications","org.freedesktop.Notifications","Notify",{"Sovel",uint(0),"sovel",title.left(120),message.left(500),QStringList{},QVariantMap{},5000},[](const QDBusMessage&){});}
