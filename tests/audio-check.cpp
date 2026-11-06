#include "backend.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv); Backend backend;
 auto wait=[&](auto action){QEventLoop loop;QTimer timer;timer.setSingleShot(true);QObject::connect(&backend,&Backend::audioChanged,&loop,&QEventLoop::quit);QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);timer.start(5000);action();loop.exec();return timer.isActive();};
 if(!wait([&]{backend.refreshAudio();}) || !backend.audio().value("available").toBool())return 2;
 const int previous=backend.audio().value("volume").toInt();const bool muted=backend.audio().value("muted").toBool();
 auto eventually=[&](auto predicate){QElapsedTimer elapsed;elapsed.start();while(!predicate()&&elapsed.elapsed()<5000){wait([&]{QTimer::singleShot(100,&backend,&Backend::refreshAudio);});}return predicate();};
 wait([&]{backend.setVolume(37);});
 bool ok=eventually([&]{return backend.audio().value("volume").toInt()==37;});
 qInfo()<<"Volume readback:"<<backend.audio()<<backend.notice();
 wait([&]{backend.setMuted(!muted);});ok=eventually([&]{return backend.audio().value("muted").toBool()==!muted;})&&ok;
 wait([&]{backend.setVolume(previous);});bool restored=eventually([&]{return backend.audio().value("volume").toInt()==previous;});
 wait([&]{backend.setMuted(muted);});restored=eventually([&]{return backend.audio().value("muted").toBool()==muted;})&&restored;
 qInfo()<<"Audio read/write:"<<ok<<"original state restored:"<<restored;
 auto waitPower=[&](auto action){QEventLoop loop;QTimer timer;timer.setSingleShot(true);QObject::connect(&backend,&Backend::powerChanged,&loop,&QEventLoop::quit);QObject::connect(&timer,&QTimer::timeout,&loop,&QEventLoop::quit);timer.start(12000);action();loop.exec();return timer.isActive();};
 bool powerOk=waitPower([&]{backend.refreshPower();})&&backend.power().value("available").toBool();
 if(powerOk){
  const auto old=backend.power().value("current").toString();
  const auto profiles=backend.power().value("profiles").toStringList();
  QString next;for(const auto &profile:profiles)if(profile!=old){next=profile;break;}
  if(!next.isEmpty()){
   powerOk=waitPower([&]{backend.setPowerProfile(next);})&&backend.power().value("current").toString()==next;
   qInfo()<<"Requested power profile:"<<next<<"Readback:"<<backend.power()<<backend.notice();
   const bool powerRestored=waitPower([&]{backend.setPowerProfile(old);})&&backend.power().value("current").toString()==old;
   powerOk=powerOk&&powerRestored;qInfo()<<"Power profile restored:"<<powerRestored;
  }
 }
 QEventLoop networkLoop;QTimer networkTimer;networkTimer.setSingleShot(true);
 QObject::connect(&backend,&Backend::networkChanged,&networkLoop,&QEventLoop::quit);QObject::connect(&networkTimer,&QTimer::timeout,&networkLoop,&QEventLoop::quit);
 networkTimer.start(12000);backend.refreshNetwork();networkLoop.exec();
 const bool networkOk=networkTimer.isActive()&&backend.networkReport().contains("Device")&&backend.networkReport().contains("Status");
 qInfo()<<"Power service integration:"<<powerOk<<"Network report integration:"<<networkOk;
 return ok&&restored&&powerOk&&networkOk?0:1;
}
