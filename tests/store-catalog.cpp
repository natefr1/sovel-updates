#include "store.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
class StoreCatalogTests:public QObject {
 Q_OBJECT
private slots:
 void noDestructiveOperationIsAllowed(){
  for(const auto &verb:QStringList{"remove","uninstall","remote-delete","remote-modify","repair","update"})QVERIFY(!Store::allowedOperation({verb,"--user","com.spotify.Client"}));
  QVERIFY(Store::allowedOperation({"install","--user","--noninteractive","--assumeyes","flathub","com.spotify.Client"}));
  QVERIFY(!Store::allowedOperation({"install","--user","--uninstall","--assumeyes","flathub","com.spotify.Client"}));
 }
 void categoriesAndRealListings(){
  Store store;store.browse("All Apps");QVERIFY(store.results().size()>=37);
  QSet<QString> seen;
  for(const auto &value:store.results()){
   const auto app=value.toMap();const auto id=app.value("id").toString();
   QVERIFY(!seen.contains(id));seen.insert(id);
   if(app.value("sourceKind")=="flatpak")QVERIFY(app.value("icon").toString().startsWith("https://dl.flathub.org/"));
   QVERIFY(!app.value("description").toString().isEmpty());
   if(app.value("sourceKind")=="flatpak")QVERIFY(!app.value("screenshots").toList().isEmpty());
  }
  QVERIFY(seen.contains("com.brave.Browser"));QVERIFY(seen.contains("md.obsidian.Obsidian"));
  for(const auto &id:QStringList{"org.kde.kdeconnect","com.storytold.ArtCraft","org.prismlauncher.PrismLauncher","org.localsend.localsend_app","io.github.flattool.Warehouse"})QVERIFY(seen.contains(id));
  store.browse("Create");QVERIFY(store.results().size()>=5);
  for(const auto &value:store.results())QVERIFY(value.toMap().value("collections").toStringList().contains("Create"));
 }
 void unlistedAppCannotStartInstallOrOpen(){
  Store store;store.browse("Discover");
  store.review("org.fake.App;touch /tmp/bad");QVERIFY(!store.busy());
  store.select("org.fake.App");QVERIFY(store.details().isEmpty());
  store.installSelected();QVERIFY(!store.busy());
 }
 void closingDuringServiceRequestIsSafe(){
  QTemporaryDir temp;QVERIFY(temp.isValid());
  QFile executable(temp.path()+"/flatpak");QVERIFY(executable.open(QIODevice::WriteOnly));
  executable.write("#!/bin/sh\nexec sleep 20\n");executable.close();
  executable.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
  const auto oldPath=qgetenv("PATH");qputenv("PATH",temp.path().toUtf8()+":"+oldPath);
  auto *store=new Store;store->browse("Discover");store->review("com.brave.Browser");
  QTRY_VERIFY_WITH_TIMEOUT(!store->findChildren<QProcess*>().isEmpty()&&store->findChildren<QProcess*>().last()->processId()>0,3000);
  QVERIFY(store->busy());delete store;qputenv("PATH",oldPath);
 }
 void openedAppSurvivesClosingStore(){
  QTemporaryDir temp;QVERIFY(temp.isValid());const auto pidFile=temp.path()+"/app.pid";
  QFile executable(temp.path()+"/flatpak");QVERIFY(executable.open(QIODevice::WriteOnly));
  executable.write(QByteArray("#!/bin/sh\nif [ \"$1\" = list ]; then printf 'com.brave.Browser\\tBrave\\n'; else echo $$ > '")+pidFile.toUtf8()+"'; exec sleep 20; fi\n");executable.close();
  executable.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
  const auto oldPath=qgetenv("PATH");qputenv("PATH",temp.path().toUtf8()+":"+oldPath);
  auto *store=new Store;store->browse("Discover");store->refreshInstalled();
  QTRY_VERIFY_WITH_TIMEOUT(store->installed().contains("com.brave.Browser"),3000);
  store->open("com.brave.Browser");
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pidFile),3000);
  QFile pidData(pidFile);QVERIFY(pidData.open(QIODevice::ReadOnly));const auto pid=pidData.readAll().trimmed();QVERIFY(pid.toLongLong()>1);
  delete store;qputenv("PATH",oldPath);
  QCOMPARE(QProcess::execute("kill",{"-0",QString::fromUtf8(pid)}),0);
  QCOMPARE(QProcess::execute("kill",{QString::fromUtf8(pid)}),0);
 }
};
QTEST_GUILESS_MAIN(StoreCatalogTests)
#include "store-catalog.moc"
