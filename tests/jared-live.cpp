#include "jared.h"
#include "backend.h"
#include "audio.h"
#include "connectivity.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QTextStream>
#include <QElapsedTimer>
#include <QThread>
#include <QProcess>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);app.setOrganizationName("Sovel");app.setApplicationName("Desktop");Backend backend;Audio audio;Connectivity radio;Jared jared(&backend,&audio,&radio);
 if(app.arguments().contains("--semantic")){
  auto wait=[&](const std::function<bool()> &condition,int timeout=300000){QElapsedTimer elapsed;elapsed.start();while(!condition()&&elapsed.elapsed()<timeout){app.processEvents();QThread::msleep(20);}return condition();};
  audio.setWatching(true);if(!wait([&]{return !audio.busy()&&!audio.outputs().isEmpty();},15000))return 29;
  jared.setAllowBasicActions(true);jared.setSpeakReplies(false);
  const auto before=jared.messages().size();jared.ask("From the live snapshot, briefly report the values named ramTotal and ramUsed with GB units.");
  if(!wait([&]{return !jared.busy()&&jared.messages().size()>before+1;})||!jared.loaded()||!jared.pendingAction().isEmpty())return 30;
  const auto reply=jared.messages().last().toMap()["text"].toString();QTextStream(stdout)<<"LIVE_MEASUREMENT_CONTEXT "<<QJsonDocument(QJsonObject{{"measurements",QJsonObject::fromVariantMap(backend.metrics())},{"reply",reply}}).toJson(QJsonDocument::Compact)<<Qt::endl;
  if(!reply.contains("GB",Qt::CaseInsensitive))return 31;
  jared.ask("Could you make the speakers a little quieter and put it at exactly 27 percent?");
  if(!wait([&]{return !jared.pendingAction().isEmpty()||(!jared.busy()&&jared.messages().last().toMap()["role"].toString()=="assistant");}))return 32;
  const auto action=jared.pendingAction();if(action["type"].toString()!="volume"||action["percent"].toInt()!=27||!action["modelSuggested"].toBool())return 33;
  QElapsedTimer pause;pause.start();while(pause.elapsed()<200){app.processEvents();QThread::msleep(10);}if(jared.pendingAction().isEmpty()||jared.busy())return 34;
  jared.rejectAction();jared.ask("Hows my ram doin?");if(jared.busy()||!jared.messages().last().toMap()["text"].toString().contains("RAM"))return 35;
  jared.unload();pause.restart();while(pause.elapsed()<2200){app.processEvents();QThread::msleep(10);}
  QTextStream(stdout)<<"REAL_LOCAL_CONTEXT_AND_MODEL_VOLUME_PROPOSAL_REQUIRE_APPROVAL_PASSED"<<Qt::endl;return 0;
 }
 if(app.arguments().contains("--download-only")){
  QObject::connect(&jared,&Jared::changed,&app,[&]{if(!jared.busy()&&jared.downloadProgress()<0){QTextStream(stdout)<<"MODEL_DOWNLOAD_RESULT "<<jared.modelReady()<<" "<<jared.status()<<Qt::endl;app.exit(jared.modelReady()?0:20);}});
  QTimer::singleShot(0,&jared,&Jared::downloadModel);QTimer::singleShot(600000,&app,[&]{app.exit(21);});return app.exec();
 }
 if(app.arguments().contains("--actions-only")){
  auto wait=[&](const std::function<bool()> &condition){QElapsedTimer elapsed;elapsed.start();while(!condition()&&elapsed.elapsed()<15000){app.processEvents();QThread::msleep(20);}return condition();};
  auto output=[&]{for(auto value:audio.outputs())if(value.toMap()["name"].toString()==audio.defaultOutput())return value.toMap();return QVariantMap();};
  audio.setWatching(true);if(!wait([&]{return !audio.busy()&&!audio.outputs().isEmpty();}))return 10;
  const auto before=output()["volume"].toInt();const int target=60;
  jared.setAllowBasicActions(false);jared.ask("Make my Volume 60%");
  if(jared.pendingAction()["type"].toString()!="volume"||output()["volume"].toInt()!=before||jared.loaded())return 11;
  jared.approveAction();if(!wait([&]{return !jared.busy()&&qAbs(output()["volume"].toInt()-target)<=1;}))return 12;
  jared.ask("Set volume to 80%");jared.rejectAction();audio.refresh();if(!wait([&]{return !audio.busy();})||qAbs(output()["volume"].toInt()-target)>1)return 13;
  jared.ask("Switch audio to my monitor");if(!wait([&]{return !jared.pendingAction().isEmpty();}))return 14;
  const auto selected=jared.pendingAction()["name"].toString();if(selected.isEmpty())return 15;
  jared.approveAction();if(!wait([&]{return !jared.busy()&&audio.defaultOutput()==selected;}))return 16;
  audio.setWatching(false);QTextStream(stdout)<<"REAL_JARED_APPROVAL_VOLUME_OUTPUT_AND_REJECTION_PASSED"<<Qt::endl;return 0;
 }
 bool started=false,received=false;
 QObject::connect(&jared,&Jared::changed,&app,[&]{
  if(!started||received||jared.busy())return;
  for(auto message:jared.messages()){auto m=message.toMap();if(m["role"].toString()=="assistant"&&m["text"].toString().contains("LOCAL_MODEL_OK")){
   received=true;const bool actual=jared.loaded()&&jared.modelReady()&&jared.pendingAction().isEmpty();
   QTextStream(stdout)<<QJsonDocument(QJsonObject{{"actual_local_qwen_response",actual},{"reply",m["text"].toString()},{"model","Qwen3-1.7B-Q8_0"},{"shell_actions_requested",false}}).toJson(QJsonDocument::Compact)<<Qt::endl;
   jared.unload();QTimer::singleShot(2200,&app,[&]{app.exit(actual&&!jared.loaded()?0:2);});return;
  }}
 });
 QTimer::singleShot(0,&app,[&]{started=true;jared.ask("Reply with exactly the text LOCAL_MODEL_OK and nothing else.");});
 QTimer::singleShot(360000,&app,[&]{QTextStream(stderr)<<"Local inference check failed: "<<jared.status()<<Qt::endl;app.exit(3);});return app.exec();
}
