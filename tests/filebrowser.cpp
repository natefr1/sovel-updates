#include "filebrowser.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
class FileBrowserTests:public QObject {
 Q_OBJECT
 QTemporaryDir root{QDir::homePath()+"/sovel-files-test-XXXXXX"};
 void write(const QString &name,const QByteArray &data){QFile file(root.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(data);}
private slots:
 void initTestCase(){QVERIFY(root.isValid());qputenv("XDG_DATA_HOME",root.filePath("xdg").toUtf8());qputenv("XDG_STATE_HOME",root.filePath("state").toUtf8());QSettings::setPath(QSettings::NativeFormat,QSettings::UserScope,root.filePath("settings"));QVERIFY(QDir(root.path()).mkdir("folder"));write("a & 'quoted'.txt","hello");write(".hidden","secret");write("package.deb","not a real package");}
 void listsFoldersFirstAndHidesHidden(){
  FileBrowser files;files.navigate(root.path());QTRY_VERIFY_WITH_TIMEOUT(!files.loading(),5000);
  QVERIFY(files.entries().size()>=3);QVERIFY(files.entries().first().toMap().value("folder").toBool());
  for(const auto &entry:files.entries())QVERIFY(entry.toMap().value("name")!=".hidden");
  files.setShowHidden(true);QTRY_VERIFY(!files.loading());bool found=false;for(const auto &entry:files.entries())found|=entry.toMap().value("name")==".hidden";QVERIFY(found);
 }
 void historyRestoresLocations(){
  FileBrowser files;files.navigate(root.path());files.navigate(root.filePath("folder"));QVERIFY(files.canBack());files.back();QCOMPARE(files.path(),root.path());files.forward();QCOMPARE(files.path(),root.filePath("folder"));files.up();QCOMPARE(files.path(),root.path());
 }
 void renameDoesNotOverwrite(){
  FileBrowser files;files.navigate(root.path());QTRY_VERIFY(!files.loading());
  files.rename(root.filePath("a & 'quoted'.txt"),"package.deb");QVERIFY(files.status().contains("already exists"));
  QFile original(root.filePath("a & 'quoted'.txt"));QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),QByteArray("hello"));
  files.rename(root.filePath("a & 'quoted'.txt"),"../outside");QVERIFY(files.status().contains("without slashes"));
 }
 void copyAndMoveUseLiteralPaths(){
  FileBrowser files;files.navigate(root.path());QTRY_VERIFY(!files.loading());files.copy({root.filePath("a & 'quoted'.txt")});
  files.navigate(root.filePath("folder"));QTRY_VERIFY(!files.loading());files.paste();QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(root.filePath("folder/a & 'quoted'.txt")),10000);
  QFile copied(root.filePath("folder/a & 'quoted'.txt"));QVERIFY(copied.open(QIODevice::ReadOnly));QCOMPARE(copied.readAll(),QByteArray("hello"));copied.close();
  const auto listed=[&](const QString &path){
   for(const auto &entry:files.entries())if(entry.toMap().value("path")==path)return true;
   return false;
  };
  QTRY_VERIFY_WITH_TIMEOUT(listed(root.filePath("folder/a & 'quoted'.txt")),10000);
  files.rename(root.filePath("folder/a & 'quoted'.txt"),"moved.txt");
  QTRY_VERIFY_WITH_TIMEOUT(listed(root.filePath("folder/moved.txt")),10000);
  files.copy({root.filePath("folder/moved.txt")},true);
  files.navigate(root.path());QTRY_VERIFY(!files.loading());files.paste();
  QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(root.filePath("moved.txt"))&&!QFile::exists(root.filePath("folder/moved.txt")),10000);
 }
 void packagesOpenInInstallerInsteadOfLaunching(){
  FileBrowser files;files.navigate(root.path());QTRY_VERIFY(!files.loading());QSignalSpy requested(&files,&FileBrowser::packageRequested);files.open(root.filePath("package.deb"));QCOMPARE(requested.size(),1);QCOMPARE(requested.first().first().toString(),root.filePath("package.deb"));
 }
 void recentAndStarredSurviveReopening(){
  const auto path=root.filePath("package.deb");
  {
   FileBrowser files;
   files.navigate(root.path());
   QTRY_VERIFY(!files.loading());
   files.open(path);
   files.toggleStar(path);
   QTRY_VERIFY(!files.loading());
  }
  FileBrowser files;
  files.navigate("sovel:recent");
  QTRY_VERIFY(!files.loading());
  QCOMPARE(files.collection(),QString("Recent"));
  QCOMPARE(files.entries().first().toMap().value("path").toString(),path);
  files.navigate("sovel:starred");
  QTRY_VERIFY(!files.loading());
  QCOMPARE(files.entries().size(),1);
  QCOMPARE(files.entries().first().toMap().value("path").toString(),path);
  files.toggleStar(path);
  QTRY_VERIFY(!files.loading());
  QVERIFY(files.entries().isEmpty());
  files.back();
  QCOMPARE(files.collection(),QString("Recent"));
 }
 void downloadPreferencesSurviveReopening(){
  {
   FileBrowser files;
   QVERIFY(!files.organizeDownloads());
   QVERIFY(files.notifyDownloads());
   files.setOrganizeDownloads(true);
   files.setNotifyDownloads(false);
  }
  FileBrowser files;
  QVERIFY(files.organizeDownloads());
  QVERIFY(!files.notifyDownloads());
 }
 void trashAndRestoreKeepContents(){
  write("recover-me.txt","recoverable data");FileBrowser files;files.navigate(root.path());QTRY_VERIFY(!files.loading());files.trash({root.filePath("recover-me.txt")});QTRY_VERIFY_WITH_TIMEOUT(!files.busy(),5000);QVERIFY2(!QFile::exists(root.filePath("recover-me.txt")),qPrintable(files.status()));
  const auto trash=root.filePath("xdg/Trash/files");files.navigate(trash);QTRY_VERIFY(!files.loading());QVERIFY(files.inTrash());const auto trashed=QDir(trash).entryList({"recover-me.txt*"},QDir::Files);QVERIFY(!trashed.isEmpty());files.restore(QDir(trash).filePath(trashed.first()));QTRY_VERIFY_WITH_TIMEOUT(!files.busy(),5000);
  QFile restored(root.filePath("recover-me.txt"));QVERIFY(restored.open(QIODevice::ReadOnly));QCOMPARE(restored.readAll(),QByteArray("recoverable data"));
 }
};
QTEST_MAIN(FileBrowserTests)
#include "filebrowser.moc"
