#include "windows.h"
#include <QCoreApplication>
#include <QTimer>
#include <QElapsedTimer>
#include <QTextStream>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);Windows windows;QElapsedTimer elapsed;elapsed.start();QTimer timer;timer.setInterval(100);int phase=0,id=0;QString title;
 QObject::connect(&timer,&QTimer::timeout,&app,[&]{
  QVariantMap entry;for(const auto &value:windows.entries())if(value.toMap().value("appId").toString()=="sovel-library")entry=value.toMap();
  if(elapsed.elapsed()>20000){QTextStream(stderr)<<"Window lifecycle timed out at phase "<<phase<<Qt::endl;app.exit(1);return;}
  if(entry.isEmpty())return;
  if(phase==0){id=entry.value("id").toInt();title=entry.value("title").toString();windows.activate(id);phase=1;return;}
  if(phase==1&&entry.value("active").toBool()){if(!windows.activateApp({"sovel-library"},true)){app.exit(2);return;}phase=2;return;}
  if(phase==2&&entry.value("minimized").toBool()){if(entry.value("id").toInt()!=id){app.exit(3);return;}windows.activateApp({"sovel-library"},false);phase=3;return;}
  if(phase==3&&!entry.value("minimized").toBool()&&entry.value("active").toBool()){
   if(entry.value("id").toInt()!=id||entry.value("title").toString()!=title){app.exit(4);return;}
   int count=0;for(const auto &value:windows.entries())if(value.toMap().value("appId").toString()=="sovel-library")++count;
   if(count!=1){app.exit(5);return;}QTextStream(stdout)<<"SAME_WINDOW_MINIMIZE_RESTORE_PASSED id="<<id<<Qt::endl;app.exit(0);
  }
 });timer.start();return app.exec();
}
