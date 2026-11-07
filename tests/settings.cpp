#include "settingsservice.h"
#include "sovelmode.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
class SettingsTests:public QObject {
 Q_OBJECT
 QTemporaryDir temporary;
private slots:
 void initTestCase(){SovelMode::setForTesting(false);SovelMode::setAssistantForTesting(false);qputenv("XDG_CONFIG_HOME",temporary.path().toUtf8());qputenv("XDG_DATA_HOME",(temporary.path()+"/data").toUtf8());}
 void canonicalNavigationAndValidPreferences(){
  SettingsService service(nullptr,false);QCOMPARE(service.pages().size(),29); /* Normal Mode after setup: no JARED page */ QSet<QString> seen;for(auto page:service.pages()){const auto id=page.toMap()["id"].toString();QVERIFY(!id.isEmpty());QVERIFY(!seen.contains(id));seen.insert(id);}QVERIFY(service.definitions().size()>=50);
  QVERIFY(!service.setPreference("shell-command","rm -rf /"));QVERIFY(!service.setPreference("appearance.accent","url(file:///etc/passwd)"));QVERIFY(!service.setPreference("input.pointerSpeed",101));QVERIFY(!service.setPreference("bar.visible","yes"));
  QVERIFY(service.setPreference("appearance.mode","light"));QVERIFY(service.colors()["light"].toBool());QVERIFY(service.setPreference("appearance.mode","dark"));QVERIFY(!service.colors()["light"].toBool());
 }
 void existingAliasesAndFavoritesPersist(){
  {SettingsService service(nullptr,false);QVERIFY(service.setPreference("reduceMotion",true));service.favorite("network",true);service.favorite("not-a-page",true);service.visit("sound");}
  QSettings old("Sovel","Desktop");old.sync();QVERIFY(old.value("motion/reduced").toBool());SettingsService reader(nullptr,false);QVERIFY(reader.preferences()["favorites"].toStringList().contains("network"));QVERIFY(!reader.preferences()["favorites"].toStringList().contains("not-a-page"));QCOMPARE(reader.preferences()["recent"].toStringList().first(),QString("sound"));
 }
 void profilesCannotGrantActionPermissions(){
  SettingsService service(nullptr,false);const auto path=temporary.path()+"/profile.json";QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(QJsonObject{{"format","sovel-appearance-1"},{"preferences",QJsonObject{{"jared.allowBasic",true}}}}).toJson());file.close();QVERIFY(service.readProfile(QUrl::fromLocalFile(path)).contains("error"));QVERIFY(!service.applyProfile({{"preferences",QVariantMap{{"jared.allowBasic",true}}}}));
  QVERIFY(service.setPreference("appearance.accent","#9966CC"));QVERIFY(service.exportProfile(QUrl::fromLocalFile(path)));const auto proposal=service.readProfile(QUrl::fromLocalFile(path));QVERIFY(!proposal.contains("error"));QVERIFY(service.setPreference("appearance.accent","#112233"));QVERIFY(service.applyProfile(proposal));QCOMPARE(service.preferences()["appearance.accent"].toString(),QString("#9966CC"));
 }
 void diagnosticExportRequiresConsent(){
  SettingsService service(nullptr,false);const auto path=QUrl::fromLocalFile(temporary.path()+"/diagnostic.json");
  QVERIFY(service.setPreference("privacy.diagnostics",false));QVERIFY(!service.exportDiagnostics(path));QVERIFY(!QFile::exists(path.toLocalFile()));
  QVERIFY(service.setPreference("privacy.diagnostics",true));QVERIFY(service.exportDiagnostics(path));QFile file(path.toLocalFile());QVERIFY(file.open(QIODevice::ReadOnly));const auto object=QJsonDocument::fromJson(file.readAll()).object();QCOMPARE(object["format"].toString(),QString("sovel-diagnostics-1"));QVERIFY(!object.contains("chat"));QVERIFY(!object.contains("network"));
 }
};
QTEST_GUILESS_MAIN(SettingsTests)
#include "settings.moc"
