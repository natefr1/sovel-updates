#include "packageinstaller.h"
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QProcess>
using Tx=PackageKit::Transaction;
namespace {
const QString Service="org.freedesktop.PackageKit",Root="/org/freedesktop/PackageKit",Interface=Service+".Transaction";
class Packages:public QDBusVirtualObject {
public:
 int next=0,calls=0;qulonglong flags=0;QStringList files;bool removeEssential=false,failSignature=false;
 QString introspect(const QString &)const override{return {};}
 void signal(const QDBusConnection &bus,const QString &path,const QString &name,const QVariantList &values){auto signal=QDBusMessage::createSignal(path,Interface,name);signal.setArguments(values);bus.send(signal);}
 bool handleMessage(const QDBusMessage &message,const QDBusConnection &bus)override{
  if(message.member()=="CreateTransaction"){bus.send(message.createReply(QVariantList{QVariant::fromValue(QDBusObjectPath(Root+"/transactions/t"+QString::number(++next)))}));return true;}
  if(message.interface()=="org.freedesktop.DBus.Properties"&&message.member()=="GetAll"){
   QVariantMap values=message.path()==Root?QVariantMap{{"VersionMajor",uint(1)},{"VersionMinor",uint(3)},{"VersionMicro",uint(4)},{"BackendName","aptcc"}}:QVariantMap{{"AllowCancel",true},{"Percentage",uint(40)}};
   bus.send(message.createReply(QVariantList{values}));return true;
  }
  if(message.member()=="SetHints"){bus.send(message.createReply());return true;}
  if(message.member()!="InstallFiles")return false;
  ++calls;flags=message.arguments().at(0).toULongLong();files=message.arguments().at(1).toStringList();bus.send(message.createReply());const auto path=message.path();
  QTimer::singleShot(30,this,[this,bus,path]{
   if(failSignature){signal(bus,path,"ErrorCode",{uint(Tx::ErrorBadGpgSignature),"Signature rejected"});signal(bus,path,"Finished",{uint(Tx::ExitFailed),uint(0)});return;}
   signal(bus,path,"Package",{uint(removeEssential?Tx::InfoRemoving:Tx::InfoInstalling),removeEssential?"labwc;1;amd64;repo":"sovel-test-download;1;all;local","Test package"});
   signal(bus,path,"Finished",{uint(Tx::ExitSuccess),uint(0)});
  });return true;
 }
};
}
class PackageInstallerTests:public QObject {
 Q_OBJECT
 QTemporaryDir fixture;
 QDBusConnection server=QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"),"sovel-local-package-fixture");
 Packages service;QString package;
private slots:
 void initTestCase(){
  qputenv("DBUS_SYSTEM_BUS_ADDRESS",qgetenv("DBUS_SESSION_BUS_ADDRESS"));QVERIFY(fixture.isValid());QVERIFY(server.registerService(Service));QVERIFY(server.registerVirtualObject("/",&service,QDBusConnection::SubPath));
  QDir().mkpath(fixture.filePath("build/DEBIAN"));QFile control(fixture.filePath("build/DEBIAN/control"));QVERIFY(control.open(QIODevice::WriteOnly));control.write("Package: sovel-test-download\nVersion: 1.0\nArchitecture: all\nMaintainer: Test\nDescription: Benign local package fixture\n");control.close();
  package=fixture.filePath("download.deb");QCOMPARE(QProcess::execute("dpkg-deb",{"--build",fixture.filePath("build"),package}),0);
 }
 void init(){service.calls=0;service.removeEssential=service.failSignature=false;service.files.clear();}
 void downloadIsStagedAndRequiresReview(){
  PackageInstaller installer;installer.load(package);QTRY_VERIFY_WITH_TIMEOUT(!installer.busy(),10000);QVERIFY2(installer.error().isEmpty(),qPrintable(installer.error()));QCOMPARE(installer.details().value("Package").toString(),QString("sovel-test-download"));
  QVERIFY(installer.details().value("file").toString()!=package);QCOMPARE(installer.details().value("sha256").toString().size(),64);
  installer.install();QTest::qWait(50);QCOMPARE(service.calls,0);
  installer.review();QTRY_VERIFY_WITH_TIMEOUT(!installer.busy(),5000);QVERIFY2(installer.reviewed(),qPrintable(installer.error()));QCOMPARE(service.flags,qulonglong(Tx::TransactionFlagSimulate));QVERIFY(QFile::exists(service.files.first()));
  installer.install();QTRY_VERIFY_WITH_TIMEOUT(!installer.busy(),5000);QVERIFY(installer.installed());QCOMPARE(service.calls,2);
 }
 void brokenArchivesAreRejected(){
  QFile broken(fixture.filePath("broken.deb"));QVERIFY(broken.open(QIODevice::WriteOnly));broken.write("not a Debian package");broken.close();PackageInstaller installer;installer.load(broken.fileName());QTRY_VERIFY_WITH_TIMEOUT(!installer.busy(),10000);QVERIFY(!installer.error().isEmpty());installer.install();QCOMPARE(service.calls,0);
 }
 void essentialRemovalIsBlocked(){
  service.removeEssential=true;PackageInstaller installer;installer.load(package);QTRY_VERIFY(!installer.busy());installer.review();QTRY_VERIFY(!installer.busy());QVERIFY(!installer.reviewed());QVERIFY(installer.error().contains("essential"));installer.install();QCOMPARE(service.calls,1);
 }
 void failedSignatureNeverReportsInstalled(){
  PackageInstaller installer;installer.load(package);QTRY_VERIFY(!installer.busy());installer.review();QTRY_VERIFY(!installer.busy());QVERIFY(installer.reviewed());service.failSignature=true;installer.install();QTRY_VERIFY(!installer.busy());QVERIFY(!installer.installed());QVERIFY(installer.error().contains("Signature"));
 }
};
QTEST_GUILESS_MAIN(PackageInstallerTests)
#include "packageinstaller.moc"
