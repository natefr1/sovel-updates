#include "workspace.h"
#include "backend.h"
#include "settingsservice.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
class WorkspaceTests:public QObject {
 Q_OBJECT
 QTemporaryDir temporary;
private slots:
 void initTestCase(){qputenv("XDG_CONFIG_HOME",temporary.path().toUtf8());qputenv("XDG_DATA_HOME",(temporary.path()+"/data").toUtf8());}
 void layoutValidationPersistenceAndEmptyState(){
  Backend backend;SettingsService settings(nullptr,false);
  {Workspace workspace(&backend,&settings,false);QCOMPARE(workspace.registry().size(),23); // Normal Mode: JARED, projects and AI monitor widgets are private-onlyQVERIFY(workspace.addWidget("unknown").isEmpty());
   const auto id=workspace.addWidget("todo");QVERIFY(!id.isEmpty());QVERIFY(workspace.configureWidget(id,"large",65,15));QVERIFY(workspace.moveWidget(id,.25,.75));
   QVERIFY(workspace.setWidgetData(id,{{"title","Tasks"},{"items",QVariantList{QVariantMap{{"text","Keep existing Store apps"},{"done",false}}}}}));
   QVERIFY(!workspace.configureWidget(id,"impossible",65,15));QVERIFY(!workspace.setWidgetData("unknown",{}));
   QVERIFY(!workspace.setWidgetData(id,{{"notes",QString(33000,'x')}}));
   const auto before=workspace.layout();QVERIFY(!workspace.saveLayout({QVariantMap{{"type","run-shell"}}}));QCOMPARE(workspace.layout(),before);
  }
  {Workspace reader(&backend,&settings,false);bool found=false;for(auto value:reader.layout()){auto w=value.toMap();if(w["type"]=="todo"){found=true;QCOMPARE(w["size"].toString(),QString("large"));QCOMPARE(w["x"].toDouble(),.25);QVERIFY(reader.data().contains(w["id"].toString()));}}QVERIFY(found);QVERIFY(reader.saveLayout({}));}
  Workspace empty(&backend,&settings,false);QVERIFY(empty.layout().isEmpty());
  QFile file(temporary.path()+"/Sovel/Workspace.json");QVERIFY(file.permissions().testFlag(QFile::ReadOwner));QVERIFY(!file.permissions().testFlag(QFile::ReadOther));
 }
 void arithmeticAndPrivacy(){
  Backend backend;SettingsService settings(nullptr,false);Workspace workspace(&backend,&settings,false);
  QCOMPARE(workspace.calculate("12 * 4"),QString("48"));QCOMPARE(workspace.calculate("1 / 0"),QString("Cannot divide by zero."));QVERIFY(workspace.calculate("process.exit()").startsWith("Use two numbers"));
  workspace.searchCity("Chicago");QVERIFY(workspace.cities().isEmpty());QVERIFY(workspace.status().contains("Enable weather"));QVERIFY(workspace.usage().isEmpty());workspace.clearUsage();QVERIFY(workspace.usage().isEmpty());
 }
 void customGeometryPersistsIndependentlyAndCanBeUndone(){
  Backend backend;
  SettingsService settings(nullptr,false);
  Workspace workspace(&backend,&settings,false);
  const auto id=workspace.addWidget("system");
  QVERIFY(!id.isEmpty());
  QVERIFY(workspace.placeWidget(id,"HDMI-A-1",.25,.30,468,206));
  QVERIFY(workspace.placeWidget(id,"DP-1",.70,.20,190,366));
  QVERIFY(workspace.placeWidget(id,"HDMI-A-1",.40,.10,300,300));
  QVERIFY(!workspace.placeWidget(id,"DP-1",.50,.50,140,310));
  QVERIFY(workspace.undoWidgetPlacement("HDMI-A-1"));
  Workspace reader(&backend,&settings,false);
  const auto widget=reader.layout().last().toMap();
  QCOMPARE(widget.value("id").toString(),id);
  const auto placements=widget.value("placements").toMap();
  QCOMPARE(placements.value("HDMI-A-1").toMap().value("width").toInt(),468);
  QCOMPARE(placements.value("HDMI-A-1").toMap().value("x").toDouble(),.25);
  QCOMPARE(placements.value("DP-1").toMap().value("height").toInt(),366);
  QCOMPARE(placements.value("DP-1").toMap().value("x").toDouble(),.70);
  QVERIFY(workspace.undoWidgetPlacement("HDMI-A-1"));
  QVERIFY(!workspace.layout().last().toMap().value("placements").toMap().contains("HDMI-A-1"));
  QVERIFY(workspace.layout().last().toMap().value("placements").toMap().contains("DP-1"));
 }
 void clearedUsageCannotReturnFromAnotherProcess(){
  QFile existing(temporary.path()+"/Sovel/Workspace.json");QVERIFY(existing.open(QIODevice::ReadOnly));auto content=QJsonDocument::fromJson(existing.readAll()).object();existing.close();content["usage"]=QJsonObject{{QDate::currentDate().toString(Qt::ISODate),QJsonObject{{"com.example.Previous",60}}}};QVERIFY(existing.open(QIODevice::WriteOnly|QIODevice::Truncate));existing.write(QJsonDocument(content).toJson());existing.close();
  Backend backend;SettingsService settings(nullptr,false);QVERIFY(settings.setPreference("usage.enabled",true));Workspace desktop(&backend,&settings,true);desktop.setForeground("com.example.Test");QTest::qWait(2200);QVERIFY(!desktop.usage().isEmpty());
  QVERIFY(settings.setPreference("usage.enabled",false));Workspace editor(&backend,&settings,false);editor.clearUsage();QTest::qWait(2300);QVERIFY(desktop.usage().isEmpty());QVERIFY(editor.usage().isEmpty());
 }
};
QTEST_GUILESS_MAIN(WorkspaceTests)
#include "workspace.moc"
