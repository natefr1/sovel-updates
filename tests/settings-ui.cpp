#include "settingsservice.h"
#include <QtTest>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQmlPropertyMap>
#include <QJSValue>
#include <QTemporaryDir>
// UI fixtures deliberately expose no host actions. Native radio/audio suites
// exercise the real services independently, including failure states.
class SurfaceFixture:public QQmlPropertyMap {
 Q_OBJECT
public:
 SurfaceFixture():QQmlPropertyMap(this,nullptr){
  for(auto key:QStringList{"busy","recording","tuningAvailable","wifiBusy","wifiEnabled","wifiAvailable","bluetoothBusy","bluetoothEnabled","bluetoothAvailable","automaticChecks","checked","restartRequired","installing","canInstall","canCancel","liveSession"})insert(key,false);
  for(auto key:QStringList{"outputs","inputs","streams","cards","wifiNetworks","savedWifi","bluetoothDevices","packages"})insert(key,QVariantList{});
  for(auto key:QStringList{"defaultOutput","defaultInput","lastRecording","error","lastChecked","wallpaperUrl","notice"})insert(key,QString());
  insert("username","fixture");insert("status","No hardware in UI fixture");insert("wifiStatus","No adapter in UI fixture");insert("bluetoothStatus","No adapter in UI fixture");insert("networkReport","Ethernet fixture");
  insert("preferredRate",48000);insert("preferredQuantum",1024);insert("inputLevel",0.0);insert("progress",-1);insert("reducedMotion",true);insert("lowEffects",true);
  insert("systemInfo",QVariantMap{{"version","test"}});insert("power",QVariantMap{});insert("metrics",QVariantMap{{"cpu",12},{"ramUsed",1.4},{"diskFree",48}});
  insert("entries",QVariantList{});
  for(auto key:{"layout","registry","cities","usage"})insert(key,QVariantList{});
  for(auto key:{"data","weather","telemetry","media"})insert(key,QVariantMap{});
 }
 Q_INVOKABLE void refreshNetwork(){}
 Q_INVOKABLE void refreshPower(){}
 Q_INVOKABLE void startWifi(){}
 Q_INVOKABLE void stopWifi(){}
 Q_INVOKABLE void startBluetooth(){}
 Q_INVOKABLE void stopBluetooth(){}
 Q_INVOKABLE void setWatching(bool){}
 Q_INVOKABLE void monitorInput(const QString &,bool){}
 Q_INVOKABLE void stopRecording(){}
 Q_INVOKABLE void registerPopup(QObject*,bool){}
 Q_INVOKABLE void releasePopups(){}
signals:
 void changed();
};
class SettingsUiTests:public QObject {
 Q_OBJECT
 QTemporaryDir temporary;
 QStringList errors;
private slots:
 void initTestCase(){qputenv("XDG_CONFIG_HOME",temporary.path().toUtf8());qputenv("XDG_STATE_HOME",temporary.path().toUtf8());}
 void navigationSearchAndThemes(){
  SettingsService settings(nullptr,false);SurfaceFixture surface;QQuickView view;
  connect(view.engine(),&QQmlEngine::warnings,this,[this](const QList<QQmlError> &warnings){for(auto w:warnings)errors<<w.toString();});
  for(auto name:{"System","Radio","Audio","Updates","Chrome","Windows","Workspace"})view.rootContext()->setContextProperty(name,&surface);
  view.rootContext()->setContextProperty("SettingsService",&settings);view.rootContext()->setContextProperty("sovelWindow",&view);view.rootContext()->setContextProperty("initialSettingsPage",QString("home"));
  view.setResizeMode(QQuickView::SizeRootObjectToView);view.setSource(QUrl::fromLocalFile(QString(SOVEL_QML_DIR)+"/Settings.qml"));QCOMPARE(view.status(),QQuickView::Ready);view.show();QTest::qWait(30);auto root=view.rootObject();QVERIFY(root);
  auto navigate=[&](const QString &page){QVERIFY(QMetaObject::invokeMethod(root,"navigateNamed",Q_ARG(QVariant,page)));QTest::qWait(10);QCOMPARE(root->property("pageId").toString(),page);};
  for(auto size:{QSize(1000,680),QSize(640,480)})for(auto theme:{QString("dark"),QString("light")}){
   view.resize(size);QVERIFY(settings.setPreference("appearance.mode",theme));
   for(auto page:settings.pages()){const auto id=page.toMap()["id"].toString();navigate(id);QVERIFY(!view.grabWindow().isNull());}
  }
  auto search=root->findChild<QQuickItem*>("settings-search");QVERIFY(search);
  for(auto pair:{qMakePair("pointer speed","input"),qMakePair("network & internet","network"),qMakePair("accent color","appearance")}){
   search->setProperty("text",pair.first);search->forceActiveFocus();QTest::keyClick(&view,Qt::Key_Return);QTest::qWait(20);QCOMPARE(root->property("pageId").toString(),QString(pair.second));
  }
  search->setProperty("text","nothing-matches-this");QTest::qWait(20);for(auto item:root->findChildren<QQuickItem*>())if(item->objectName().startsWith("settings-page-"))QVERIFY(!item->isVisible());
  search->forceActiveFocus();QTest::keyClick(&view,Qt::Key_Escape);QCOMPARE(search->property("text").toString(),QString());
  navigate("home");navigate("sound");QVERIFY(QMetaObject::invokeMethod(root,"back"));QCOMPARE(root->property("pageId").toString(),QString("home"));QVERIFY(QMetaObject::invokeMethod(root,"forward"));QCOMPARE(root->property("pageId").toString(),QString("sound"));
  QVERIFY2(errors.isEmpty(),qPrintable(errors.join('\n')));
 }
};
QTEST_MAIN(SettingsUiTests)
#include "settings-ui.moc"
