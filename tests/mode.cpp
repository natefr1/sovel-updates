#include "settingsservice.h"
#include "workspace.h"
#include "backend.h"
#include "sovelmode.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QDir>
#include <QSettings>
#include <QFile>

// The Normal/Private boundary in the shell: once Private Mode is set up,
// Normal Mode never receives private pages, preferences, widget types or JARED
// controls. Before setup, JARED stays in Normal Mode.
class ModeTests:public QObject {
 Q_OBJECT
 QTemporaryDir temporary;
 static QStringList pageIds(const SettingsService &service){QStringList ids;for(const auto &page:service.pages())ids<<page.toMap()["id"].toString();return ids;}
 static bool anyItemTargets(const SettingsService &service,const QString &needle){
  for(const auto &page:service.pages())for(const auto &section:page.toMap()["sections"].toList())for(const auto &item:section.toMap()["items"].toList()){const auto entry=item.toMap();if(entry["target"]==needle||entry["key"].toString().startsWith(needle)||entry["operation"]==needle)return true;}
  return false;
 }
 static QStringList registryTypes(const Workspace &workspace){QStringList types;for(const auto &entry:workspace.registry())types<<entry.toMap()["type"].toString();return types;}
 void writeLayout(const QVariantList &layout){
  QDir().mkpath(temporary.path()+"/Sovel");QSaveFile file(temporary.path()+"/Sovel/Workspace.json");QVERIFY(file.open(QIODevice::WriteOnly));
  file.write(QJsonDocument(QJsonObject{{"layout",QJsonArray::fromVariantList(layout)}}).toJson());QVERIFY(file.commit());
 }
private slots:
 void initTestCase(){qputenv("XDG_CONFIG_HOME",temporary.path().toUtf8());qputenv("XDG_DATA_HOME",(temporary.path()+"/data").toUtf8());qputenv("XDG_RUNTIME_DIR",temporary.path().toUtf8());}
 void normalModeHasNoPrivateSettings(){
  SovelMode::setForTesting(false);SovelMode::setAssistantForTesting(false);
  SettingsService service(nullptr,false);
  const auto ids=pageIds(service);
  // The Private Workspace page stays, showing only how to open or set it up.
  QVERIFY(!ids.contains("jared"));QVERIFY(ids.contains("private"));QCOMPARE(ids.size(),29);
  QVERIFY(!anyItemTargets(service,"jared"));QVERIFY(!anyItemTargets(service,"exit-private"));QVERIFY(!anyItemTargets(service,"vault"));
  QVERIFY(!anyItemTargets(service,"private.sealAfterLock"));QVERIFY(!anyItemTargets(service,"private-audit"));QVERIFY(!anyItemTargets(service,"preset-command"));
  QVERIFY(anyItemTargets(service,"enter-private"));QVERIFY(anyItemTargets(service,"private-setup"));
  QVERIFY(!service.colors()["private"].toBool());
  QCOMPARE(service.colors()["accent"].toString(),QString("#7c9cff"));
 }
 void normalModeKeepsJaredBeforeSetup(){
  SovelMode::setForTesting(false);SovelMode::setAssistantForTesting(true);
  SettingsService service(nullptr,false);
  const auto ids=pageIds(service);
  QVERIFY(ids.contains("jared"));QCOMPARE(ids.size(),30);
  QVERIFY(anyItemTargets(service,"bar.show.jared"));
  QVERIFY(!anyItemTargets(service,"exit-private"));QVERIFY(!anyItemTargets(service,"vault"));
  Backend backend;Workspace workspace(&backend,&service,false);
  QVERIFY(registryTypes(workspace).contains("jared"));
  QVERIFY(!workspace.applyPreset("command",{"DP-1","HDMI-A-1"},"DP-1"));
  QVERIFY(!backend.privateMode());QVERIFY(backend.assistantAvailable());
 }
 void privateModeHasItsPages(){
  SovelMode::setForTesting(true);SovelMode::setAssistantForTesting(true);
  SettingsService service(nullptr,false);
  const auto ids=pageIds(service);
  QVERIFY(ids.contains("jared"));QVERIFY(ids.contains("private"));QCOMPARE(ids.size(),30);
  QVERIFY(anyItemTargets(service,"exit-private"));QVERIFY(!anyItemTargets(service,"private-setup"));
  QVERIFY(service.colors()["private"].toBool());QVERIFY(!service.colors()["light"].toBool());
  QCOMPARE(service.colors()["accent"].toString(),QString("#bf40fa"));
  // Private Mode is always dark, whatever a preference says.
  QVERIFY(service.setPreference("appearance.mode","light"));QVERIFY(!service.colors()["light"].toBool());
  QVERIFY(service.setPreference("appearance.mode","dark"));
 }
 void normalModeDropsPrivateWidgetsFromExistingLayouts(){
  // A 0.1.0-9 Normal account still has a JARED widget in its saved layout.
  writeLayout({QVariantMap{{"id","a"},{"type","clock"},{"x",.1},{"y",.1}},QVariantMap{{"id","b"},{"type","jared"},{"x",.5},{"y",.5}},QVariantMap{{"id","c"},{"type","projects-deck"},{"x",.6},{"y",.6}}});
  SovelMode::setForTesting(false);SovelMode::setAssistantForTesting(false);
  Backend backend;SettingsService settings(nullptr,false);Workspace workspace(&backend,&settings,false);
  const auto types=registryTypes(workspace);
  QVERIFY(!types.contains("jared"));QVERIFY(!types.contains("projects-deck"));QVERIFY(!types.contains("ai-monitor"));QVERIFY(types.contains("clock"));
  QCOMPARE(workspace.layout().size(),1);QCOMPARE(workspace.layout().first().toMap()["type"].toString(),QString("clock"));
  // Moving the remaining widget still saves: the private entries do not poison the layout.
  QVERIFY(workspace.moveWidget("a",.2,.2));
  QVERIFY(workspace.addWidget("jared").isEmpty());
  QVERIFY(!workspace.applyPreset("command",{"DP-1","HDMI-A-1"},"DP-1"));
  QVERIFY(workspace.assistant().isEmpty());
 }
 void normalModeStartsOnTheCity(){
  // A 0.1.0-9 account upgraded into Normal Mode: chosen violet landscape plus a photo.
  SovelMode::setForTesting(false);SovelMode::setAssistantForTesting(false);
  {QSettings old("Sovel","Desktop");old.clear();old.setValue("appearance/wallpaper","landscape");old.setValue("appearance/wallpaperChosen",true);old.setValue("appearance/photo",temporary.path()+"/emblem.png");old.sync();}
  QFile photo(temporary.path()+"/emblem.png");QVERIFY(photo.open(QIODevice::WriteOnly));photo.write("x");photo.close();
  {Backend backend;QCOMPARE(backend.wallpaper(),QString("city"));QCOMPARE(backend.wallpaperUrl(),QUrl("qrc:/assets/city.png"));}
  // After the one-time reset the person can pick again, but not the violet artwork.
  {QSettings later("Sovel","Desktop");later.setValue("appearance/wallpaper","lockroom");later.sync();}
  {Backend backend;QCOMPARE(backend.wallpaper(),QString("lockroom"));backend.setWallpaper("emblem");QCOMPARE(backend.wallpaper(),QString("city"));}
  SovelMode::setForTesting(true);
  {Backend backend;QCOMPARE(backend.wallpaper(),QString("emblem"));}
 }
 void privatePresetTargetsSecondaryDisplay(){
  writeLayout({QVariantMap{{"id","primary-clock"},{"type","clock"},{"x",.98},{"y",.1}}});
  SovelMode::setForTesting(true);SovelMode::setAssistantForTesting(true);
  Backend backend;SettingsService settings(nullptr,false);Workspace workspace(&backend,&settings,false);
  QVERIFY(registryTypes(workspace).contains("jared"));
  QVERIFY(!workspace.applyPreset("not-a-preset",{"DP-1","HDMI-A-1"},"DP-1"));
  QVERIFY(workspace.applyPreset("command",{"DP-1","HDMI-A-1"},"DP-1"));
  int onSecondary=0;bool keptPrimary=false;
  for(const auto &value:workspace.layout()){
   const auto widget=value.toMap();const auto placements=widget["placements"].toMap();
   if(widget["id"]=="primary-clock")keptPrimary=true;
   if(placements.contains("HDMI-A-1")){++onSecondary;QVERIFY(!placements.contains("DP-1"));}
  }
  QVERIFY(keptPrimary);QVERIFY(onSecondary>=6);
  // Every widget stays editable afterwards.
  const auto first=workspace.layout().last().toMap()["id"].toString();
  QVERIFY(workspace.placeWidget(first,"HDMI-A-1",.4,.4,0,0));
 }
 void cleanupTestCase(){SovelMode::setForTesting(false);}
};
QTEST_GUILESS_MAIN(ModeTests)
#include "mode.moc"
