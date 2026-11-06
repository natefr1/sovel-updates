#include "windows.h"
#include <QCoreApplication>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);Windows windows;
 QTimer::singleShot(650,&app,[&]{
  const auto args=app.arguments();
  if(args.size()>2){
   for(auto value:windows.entries()){auto row=value.toMap();if(row.value("appId").toString()!=args[2])continue;windows.activate(row["id"].toInt());
    if(args[1]=="maximize")windows.toggleMaximized();
    if(args[1]=="minimize")windows.minimizeActive();
    if(args[1]=="close")windows.closeActive();
    break;
   }
  }
  QTimer::singleShot(450,&app,[&]{QTextStream(stdout)<<QJsonDocument(QJsonArray::fromVariantList(windows.entries())).toJson(QJsonDocument::Compact)<<Qt::endl;app.quit();});
 });return app.exec();
}
