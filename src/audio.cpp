#include "audio.h"
#include <QCoreApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QDir>
#include <QDateTime>
#include <QCryptographicHash>
#include <QtEndian>
#include <cmath>
#include <cstring>
#include <memory>

namespace {
QString routeKey(const QVariantMap &stream){return "audio/routes/"+QString::fromLatin1(QCryptographicHash::hash(stream.value("appKey").toString().toUtf8(),QCryptographicHash::Sha256).toHex());}
QString kind(const QJsonObject &raw){
 auto p=raw.value("properties").toObject();auto text=(raw.value("name").toString()+" "+raw.value("description").toString()+" "+p.value("device.bus").toString()).toLower();
 if(text.contains("hdmi")||text.contains("displayport"))return text.contains("displayport")?"Monitor · DisplayPort":"Monitor · HDMI";
 if(text.contains("bluez")||text.contains("bluetooth"))return "Bluetooth";
 if(text.contains("usb"))return "USB audio";
 if(text.contains("headphone")||text.contains("headset"))return "Headphones";
 return p.value("device.class").toString()=="abstract"?"Virtual audio":"Built-in / audio device";
}
void bounded(QProcess *process,int timeout=12000){auto timer=new QTimer(process);timer->setSingleShot(true);QObject::connect(process,&QProcess::started,timer,[timer,timeout]{timer->start(timeout);});QObject::connect(timer,&QTimer::timeout,process,[process]{process->kill();});QObject::connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),timer,&QTimer::stop);}
}
Audio::Audio(QObject *parent):QObject(parent){
 m_poll.setInterval(15000);m_debounce.setSingleShot(true);m_debounce.setInterval(200);m_recordStop.setSingleShot(true);m_recordStop.setInterval(5000);m_routeTimer.setSingleShot(true);m_routeTimer.setInterval(400);
 connect(&m_routeTimer,&QTimer::timeout,this,&Audio::refresh);
 auto environment=QProcessEnvironment::systemEnvironment();environment.insert("LC_ALL","C.UTF-8");m_subscribe.setProcessEnvironment(environment);
 connect(&m_poll,&QTimer::timeout,this,&Audio::refresh);connect(&m_debounce,&QTimer::timeout,this,&Audio::refresh);
 connect(&m_subscribe,&QProcess::readyReadStandardOutput,this,[this]{const auto events=m_subscribe.readAllStandardOutput();if((events.contains(" on sink #")||events.contains(" on source #")||events.contains(" on sink-input #")||events.contains(" on source-output #")||events.contains(" on card #")||events.contains(" on server #"))&&!m_debounce.isActive())m_debounce.start();});
 connect(&m_recordStop,&QTimer::timeout,this,&Audio::stopRecording);
 connect(&m_record,&QProcess::started,this,&Audio::changed);
 connect(&m_record,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){m_recordStop.stop();monitorInput({},false);m_status="Microphone test saved locally. Play it back to check the recording.";emit changed();});
 connect(&m_record,&QProcess::errorOccurred,this,[this](QProcess::ProcessError){m_status="The microphone test could not start.";emit changed();});
 connect(&m_meter,&QProcess::readyReadStandardOutput,this,[this]{
  const auto bytes=m_meter.readAllStandardOutput();double energy=0;const auto count=bytes.size()/4;
  for(qsizetype i=0;i<count;i++){auto bits=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()+i*4));float value;std::memcpy(&value,&bits,4);if(std::isfinite(value))energy+=double(value)*value;}
  m_level=count?qBound(0.0,std::sqrt(energy/double(count))*3.0,1.0):0;emit levelChanged();
 });
}
Audio::~Audio(){for(auto *p:{&m_subscribe,&m_meter,&m_record}){p->disconnect(this);if(p->state()!=QProcess::NotRunning){p->terminate();if(!p->waitForFinished(500)){p->kill();p->waitForFinished(500);}}}for(auto *p:findChildren<QProcess*>()){p->disconnect(this);if(p->state()!=QProcess::NotRunning){p->kill();p->waitForFinished(500);}}}
QVariantMap Audio::device(const QJsonObject &raw,const QString &selected,bool input){
 const auto volumes=raw.value("volume").toObject();int peak=0;QVariantList channels;
 auto order=raw.value("channel_map").toString().split(',',Qt::SkipEmptyParts);if(order.size()!=volumes.size())order=volumes.keys();
 for(const auto &key:order){const auto channel=volumes.value(key.trimmed()).toObject();int percent=qRound(channel.value("value").toDouble()*100.0/65536.0);peak=qMax(peak,percent);channels<<QVariantMap{{"name",key.trimmed()},{"volume",percent}};}
 const auto name=raw.value("name").toString();auto p=raw.value("properties").toObject();
 const auto left=volumes.value("front-left").toObject().value("value").toDouble(),right=volumes.value("front-right").toObject().value("value").toDouble();
 return {{"id",raw.value("index").toInt(-1)},{"name",name},{"description",raw.value("description").toString(name)},{"kind",kind(raw)},{"default",name==selected},{"volume",peak},{"balance",qMax(left,right)>0?(right-left)/qMax(left,right):0},{"muted",raw.value("mute").toBool()},{"sampleSpec",raw.value("sample_specification").toString()},{"channels",channels},{"channelMap",raw.value("channel_map").toString()},{"ports",raw.value("ports").toVariant()},{"activePort",raw.value("active_port").toString()},{"input",input},{"state",raw.value("state").toString()},{"noiseFiltered",name.contains("echo",Qt::CaseInsensitive)||name.contains("noise",Qt::CaseInsensitive)||p.value("node.virtual").toString()=="true"}};
}
void Audio::json(const QStringList &args,std::function<void(const QJsonDocument&)> callback){
 auto p=new QProcess(this);bounded(p);auto env=QProcessEnvironment::systemEnvironment();env.insert("LC_ALL","C.UTF-8");p->setProcessEnvironment(env);
 auto completed=std::make_shared<bool>(false);
 auto done=[this,p,callback,completed](bool ok){if(*completed)return;*completed=true;const auto bytes=p->readAllStandardOutput();if(!ok)m_status="The audio service did not return device information. Check PipeWire in Advanced.";callback(ok?QJsonDocument::fromJson(bytes):QJsonDocument());p->deleteLater();};
 connect(p,&QProcess::errorOccurred,this,[done](QProcess::ProcessError){done(false);});connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[done](int code,QProcess::ExitStatus){done(code==0);});p->start("pactl",QStringList{"--format=json"}+args);
}
void Audio::refresh(){
 if(m_busy){m_refreshAgain=true;return;}m_busy=true;m_pending=6;emit changed();
 auto streams=std::make_shared<QVariantList>();
 auto done=[this,streams]{if(--m_pending)return;m_streams=*streams;m_busy=false;defaults();checkRoutes();emit changed();if(m_refreshAgain){m_refreshAgain=false;m_debounce.start();}};
 json({"info"},[this,done](const QJsonDocument &d){auto info=d.object();m_defaultOutput=info.value("default_sink_name").toString();m_defaultInput=info.value("default_source_name").toString();m_pipewire=info.value("server_name").toString().contains("PipeWire",Qt::CaseInsensitive);done();});
 json({"list","sinks"},[this,done](const QJsonDocument &d){m_outputs.clear();for(auto raw:d.array())m_outputs<<device(raw.toObject(),{},false);done();});
 json({"list","sources"},[this,done](const QJsonDocument &d){m_inputs.clear();for(auto raw:d.array()){const auto o=raw.toObject();const auto monitor=o.value("monitor_of_sink");if(o.value("name").toString().endsWith(".monitor")||(monitor.isDouble()&&monitor.toDouble()>=0&&monitor.toDouble()<4294967295.0))continue;m_inputs<<device(o,{},true);}done();});
 json({"list","cards"},[this,done](const QJsonDocument &d){m_cards.clear();for(auto raw:d.array()){auto o=raw.toObject();QVariantList profiles;const auto value=o.value("profiles");
   if(value.isArray())for(auto p:value.toArray()){auto v=p.toObject().toVariantMap();if(v.value("available").toString()!="no")profiles<<v;}
   else {const auto object=value.toObject();for(auto it=object.begin();it!=object.end();++it){auto v=it.value().toObject().toVariantMap();v["name"]=it.key();if(v.value("available").toString()!="no")profiles<<v;}}
   m_cards<<QVariantMap{{"name",o.value("name").toString()},{"description",o.value("properties").toObject().value("device.description").toString(o.value("name").toString())},{"profiles",profiles},{"activeProfile",o.value("active_profile").isObject()?o.value("active_profile").toObject().value("name").toString():o.value("active_profile").toString()}};
  }done();});
 for(const auto &type:{QString("sink-inputs"),QString("source-outputs")})json({"list",type},[streams,type,done](const QJsonDocument &d){for(auto raw:d.array()){auto o=raw.toObject(),p=o.value("properties").toObject();if(p.value("application.name").toString().startsWith("Sovel internal audio"))continue;auto row=device(o,{},type=="source-outputs");row["appKey"]=p.value("application.id").toString(p.value("application.process.binary").toString(p.value("application.name").toString()));row["description"]=p.value("application.name").toString(o.value("name").toString("Audio stream"));row["targetId"]=o.value(type=="sink-inputs"?"sink":"source").toInt(-1);*streams<<row;}done();});
}
void Audio::defaults(){
 m_settings.sync();
 for(auto *list:{&m_outputs,&m_inputs})for(auto &v:*list){auto row=v.toMap();row["default"]=row.value("name").toString()==(row.value("input").toBool()?m_defaultInput:m_defaultOutput);v=row;}
 m_status=m_outputs.isEmpty()?"No audio outputs are exposed by the sound service. Check device profiles below.":"Choose an output or microphone. Changes apply to this user session.";
 // WirePlumber remembers defaults itself. Reapply our preferred device only
 // when it reappears, never continuously fight a change made by another app.
 for(const auto input:{false,true}){
  const auto pref=m_settings.value(input?"audio/preferredInput":"audio/preferredOutput").toString();const auto key=input?"audio/presentInput":"audio/presentOutput";
  QStringList names;for(auto v:input?m_inputs:m_outputs)names<<v.toMap().value("name").toString();
  const auto prior=m_settings.value(key).toStringList();
  if(!pref.isEmpty()&&names.contains(pref)&&(!prior.contains(pref)||!m_haveOutputs)&&(input?m_defaultInput:m_defaultOutput)!=pref)command({input?"set-default-source":"set-default-sink",pref});
  m_settings.setValue(key,names);
 }m_haveOutputs=true;
}
bool Audio::known(const QString &name,bool input)const{for(auto v:input?m_inputs:m_outputs)if(v.toMap().value("name").toString()==name)return true;return false;}
bool Audio::knownStream(int id,bool input)const{for(auto v:m_streams){auto m=v.toMap();if(m.value("id").toInt()==id&&m.value("input").toBool()==input)return true;}return false;}
void Audio::command(const QStringList &args,std::function<void(bool)> callback){
 auto p=new QProcess(this);bounded(p);auto complete=std::make_shared<bool>(false);auto done=[this,p,callback,complete](bool ok){if(*complete)return;*complete=true;if(!ok)m_status="The audio service could not apply that change. The device may have disconnected.";if(callback)callback(ok);if(ok)m_debounce.start();else emit changed();p->deleteLater();};
 connect(p,&QProcess::errorOccurred,this,[done](QProcess::ProcessError){done(false);});connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[done](int code,QProcess::ExitStatus){done(code==0);});p->start("pactl",args);
}
void Audio::setWatching(bool enabled){m_watchers=qMax(0,m_watchers+(enabled?1:-1));m_watching=m_watchers>0;if(m_watching){if(m_subscribe.state()==QProcess::NotRunning)m_subscribe.start("pactl",{"subscribe"});m_poll.start();refresh();}else{m_poll.stop();m_subscribe.terminate();monitorInput({},false);stopRecording();}}
void Audio::selectOutput(const QString &name){if(!known(name))return;command({"set-default-sink",name},[this,name](bool ok){if(ok){m_settings.setValue("audio/preferredOutput",name);m_settings.sync();}});}
void Audio::selectInput(const QString &name){if(!known(name,true))return;command({"set-default-source",name},[this,name](bool ok){if(ok){m_settings.setValue("audio/preferredInput",name);m_settings.sync();}});}
void Audio::volume(const QString &name,int percent,bool input){if(!known(name,input))return;QStringList args{input?"set-source-volume":"set-sink-volume",name};for(auto v:input?m_inputs:m_outputs){auto d=v.toMap();if(d.value("name").toString()!=name)continue;const int peak=d.value("volume").toInt();const auto channels=d.value("channels").toList();if(channels.size()>1&&peak>0)for(auto channel:channels)args<<QString::number(qRound(qBound(0,percent,100)*double(channel.toMap().value("volume").toInt())/peak))+"%";else args<<QString::number(qBound(0,percent,100))+"%";break;}command(args);}
void Audio::mute(const QString &name,bool value,bool input){if(known(name,input))command({input?"set-source-mute":"set-sink-mute",name,value?"1":"0"});}
void Audio::balance(const QString &name,double value){for(auto v:m_outputs){auto d=v.toMap();if(d.value("name").toString()!=name)continue;auto channels=d.value("channels").toList();if(channels.size()!=2)return;value=qBound(-1.0,value,1.0);const auto base=qBound(0,d.value("volume").toInt(),100);QStringList args{"set-sink-volume",name};for(auto ch:channels){const auto left=ch.toMap().value("name").toString().contains("left");const double factor=left?(value>0?1-value:1):(value<0?1+value:1);args<<QString::number(qRound(base*factor))+"%";}command(args);return;}}
void Audio::streamVolume(int id,int value,bool input){if(knownStream(id,input))command({input?"set-source-output-volume":"set-sink-input-volume",QString::number(id),QString::number(qBound(0,value,100))+"%"});}
void Audio::streamMute(int id,bool value,bool input){if(knownStream(id,input))command({input?"set-source-output-mute":"set-sink-input-mute",QString::number(id),value?"1":"0"});}
void Audio::moveStream(int id,const QString &name,bool input){
 if(!known(name,input)||!knownStream(id,input))return;
 const auto key=QString::number(id)+(input?"/input":"/output");
 QString appKey;for(auto v:m_streams){auto row=v.toMap();if(row["id"].toInt()==id&&row["input"].toBool()==input)appKey=row["appKey"].toString();}
 m_routes[key]={{"id",id},{"name",name},{"input",input},{"appKey",appKey},{"attempts",0},{"waiting",false},{"token",QDateTime::currentMSecsSinceEpoch()}};requestRoute(key);
}
void Audio::requestRoute(const QString &key){
 if(!m_routes.contains(key))return;auto &route=m_routes[key];route["attempts"]=route["attempts"].toInt()+1;route["waiting"]=true;const auto token=route["token"];
 command({route["input"].toBool()?"move-source-output":"move-sink-input",QString::number(route["id"].toInt()),route["name"].toString()},[this,key,token](bool ok){
  if(!m_routes.contains(key)||m_routes[key]["token"]!=token)return;
  if(!ok){m_routes.remove(key);emit changed();return;}m_routes[key]["waiting"]=false;m_routeTimer.start();
 });
}
void Audio::checkRoutes(){
 for(const auto &key:m_routes.keys()){
  const auto route=m_routes.value(key);const bool input=route["input"].toBool();QVariantMap stream,target;
  for(auto value:m_streams){auto row=value.toMap();if(row["id"]==route["id"]&&row["input"].toBool()==input&&row["appKey"]==route["appKey"])stream=row;}
  for(auto value:input?m_inputs:m_outputs){auto row=value.toMap();if(row["name"]==route["name"])target=row;}
  if(stream.isEmpty()||target.isEmpty()){m_routes.remove(key);m_status="The app or audio device disconnected before routing was confirmed.";continue;}
  if(stream["targetId"].toInt()==target["id"].toInt()){m_routes.remove(key);m_status="The app is now using the selected audio device.";continue;}
  if(route["waiting"].toBool())continue;
  if(route["attempts"].toInt()>=5){m_routes.remove(key);m_status="The audio service did not keep the requested app route. Check the device or retry.";continue;}
  requestRoute(key);
 }
 if(!m_routes.isEmpty())m_routeTimer.start();
}
void Audio::profile(const QString &name,const QString &profileName){for(auto c:m_cards){auto card=c.toMap();if(card.value("name").toString()!=name)continue;for(auto p:card.value("profiles").toList())if(p.toMap().value("name").toString()==profileName){command({"set-card-profile",name,profileName});return;}}}
void Audio::monitorInput(const QString &name,bool enabled){if(m_meter.state()!=QProcess::NotRunning){m_meter.kill();m_meter.waitForFinished(300);}m_level=0;emit levelChanged();if(enabled&&known(name,true))m_meter.start("parec",{"--client-name=Sovel internal audio","--device="+name,"--format=float32le","--rate=16000","--channels=1","--latency-msec=40","--raw"});}
void Audio::recordTest(const QString &name){if(!known(name,true)||recording())return;const auto folder=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/audio-tests";if(!QDir().mkpath(folder))return;m_lastRecording=folder+"/microphone-test.wav";m_record.start("parecord",{"--client-name=Sovel internal audio","--device="+name,"--file-format=wav","--rate=48000","--channels=1",m_lastRecording});monitorInput(name,true);m_recordStop.start();m_status="Recording a five-second microphone test locally…";emit changed();}
void Audio::stopRecording(){m_recordStop.stop();if(recording())m_record.terminate();}
void Audio::playRecording(){if(m_lastRecording.isEmpty()||recording())return;auto p=new QProcess(this);bounded(p,20000);connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),p,&QObject::deleteLater);p->start("paplay",{"--client-name=Sovel internal audio",m_lastRecording});}
void Audio::testSpeaker(const QString &name,int channel){
 if(!known(name)||channel<0||channel>1)return;QByteArray samples;const int frames=16800;for(int i=0;i<frames;i++)for(int c=0;c<2;c++){qint16 sample=c==channel?qint16(std::sin(2*3.141592653589793*440*i/48000.0)*2500):0;auto le=qToLittleEndian(sample);samples.append(reinterpret_cast<const char*>(&le),2);}
 auto file=new QTemporaryFile(this);if(!file->open()){delete file;return;}file->write(samples);file->flush();auto p=new QProcess(this);bounded(p,10000);connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[p,file](int,QProcess::ExitStatus){p->deleteLater();file->deleteLater();});p->start("paplay",{"--latency-msec=40","--client-name=Sovel internal audio","--device="+name,"--raw","--format=s16le","--rate=48000","--channels=2","--channel-map=front-left,front-right",file->fileName()});
}
void Audio::forceMetadata(const QString &key,int value){if(!m_pipewire)return;auto p=new QProcess(this);bounded(p);connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,p](int code,QProcess::ExitStatus){m_status=code==0?"Audio engine preference applied. Automatic restores device negotiation.":"This audio engine cannot apply that preference.";emit changed();p->deleteLater();});p->start("pw-metadata",{"-n","settings","0",key,QString::number(value)});}
void Audio::setPreferredRate(int value){if(!QList<int>{0,44100,48000,96000,192000}.contains(value)||!m_pipewire)return;m_settings.setValue("audio/rate",value);forceMetadata("clock.force-rate",value);emit changed();}
void Audio::setPreferredQuantum(int value){if(!QList<int>{0,128,256,512,1024,2048}.contains(value)||!m_pipewire)return;m_settings.setValue("audio/quantum",value);forceMetadata("clock.force-quantum",value);emit changed();}
void Audio::restorePreferences(){json({"info"},[this](const QJsonDocument &d){m_pipewire=d.object().value("server_name").toString().contains("PipeWire",Qt::CaseInsensitive);if(preferredRate())forceMetadata("clock.force-rate",preferredRate());if(preferredQuantum())forceMetadata("clock.force-quantum",preferredQuantum());});}
void Audio::openEqualizer(){const auto executable=QStandardPaths::findExecutable("easyeffects");if(!executable.isEmpty())QProcess::startDetached(executable,{});else if(QFile::exists(QDir::homePath()+"/.local/share/flatpak/exports/share/applications/com.github.wwmm.easyeffects.desktop")||QFile::exists("/var/lib/flatpak/exports/share/applications/com.github.wwmm.easyeffects.desktop"))QProcess::startDetached("flatpak",{"run","com.github.wwmm.easyeffects"});else{m_status="Search for EasyEffects in Store for equalizer and noise suppression. No audio filter is enabled silently.";emit changed();QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--app","store"});}}
