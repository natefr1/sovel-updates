#include "updates.h"
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QTemporaryDir>
#include <QtTest>
using Tx=PackageKit::Transaction;
namespace {
const QString Service="org.freedesktop.PackageKit",Root="/org/freedesktop/PackageKit",Interface=Service+".Transaction";
class PackageService : public QDBusVirtualObject {
public:
    int next=0,refreshes=0,lists=0,installs=0,cancels=0;
    qulonglong flags=0;QStringList ids,hints;QString active;
    bool hold=false,signatureFailure=false,networkFailure=false;
    QString introspect(const QString &) const override{return {};}
    void signal(const QDBusConnection &bus,const QString &path,const QString &name,const QVariantList &args){
        auto m=QDBusMessage::createSignal(path,Interface,name);m.setArguments(args);bus.send(m);
    }
    void finish(const QDBusConnection &bus,const QString &path,Tx::Exit code=Tx::ExitSuccess){signal(bus,path,"Finished",{uint(code),uint(0)});}
    bool handleMessage(const QDBusMessage &m,const QDBusConnection &bus) override {
        if(m.member()=="CreateTransaction"){
            active=Root+"/transactions/t"+QString::number(++next);
            bus.send(m.createReply(QVariantList{QVariant::fromValue(QDBusObjectPath(active))}));return true;
        }
        if(m.interface()=="org.freedesktop.DBus.Properties"&&m.member()=="GetAll"){
            QVariantMap values=m.path()==Root?QVariantMap{{"VersionMajor",uint(1)},{"VersionMinor",uint(3)},{"VersionMicro",uint(4)},{"BackendName","aptcc"}}:QVariantMap{{"AllowCancel",true},{"Percentage",uint(25)}};
            bus.send(m.createReply(QVariantList{values}));return true;
        }
        if(m.member()=="SetHints"){hints=m.arguments().value(0).toStringList();bus.send(m.createReply());return true;}
        if(m.member()=="Cancel"){++cancels;bus.send(m.createReply());finish(bus,m.path(),Tx::ExitCancelled);return true;}
        if(m.member()!="RefreshCache"&&m.member()!="GetUpdates"&&m.member()!="UpdatePackages")return false;
        bus.send(m.createReply());
        auto path=m.path();auto member=m.member();
        if(member=="RefreshCache")++refreshes;
        if(member=="GetUpdates")++lists;
        if(member=="UpdatePackages"){++installs;flags=m.arguments().at(0).toULongLong();ids=m.arguments().at(1).toStringList();}
        if(hold)return true;
        QTimer::singleShot(50,this,[this,bus,path,member]{
            if(networkFailure){signal(bus,path,"ErrorCode",{uint(Tx::ErrorNoNetwork),"Offline"});finish(bus,path,Tx::ExitFailed);return;}
            if(signatureFailure&&member=="UpdatePackages"){
                signal(bus,path,"ErrorCode",{uint(Tx::ErrorBadGpgSignature),"Signature rejected"});finish(bus,path,Tx::ExitFailed);return;
            }
            if(member=="GetUpdates"){
                const QString id="sovel-desktop;0.1.1-1;amd64;sovel";
                signal(bus,path,"Package",{uint(Tx::InfoSecurity),id,"Sovel security and desktop update"});
                signal(bus,path,"Package",{uint(Tx::InfoSecurity),id,"Duplicate"});
                signal(bus,path,"Package",{uint(Tx::InfoUntrusted),"bad;1;amd64;unknown","Untrusted"});
                signal(bus,path,"Package",{uint(Tx::InfoBlocked),"blocked;1;amd64;sovel","Blocked"});
            }
            if(member=="UpdatePackages"){signal(bus,path,"ItemProgress",{"sovel-desktop;0.1.1-1;amd64;sovel",uint(Tx::StatusDownload),uint(100)});signal(bus,path,"RequireRestart",{uint(Tx::RestartSystem),"sovel-desktop;0.1.1-1;amd64;sovel"});}
            finish(bus,path);
        });return true;
    }
    void reset(){refreshes=lists=installs=cancels=0;flags=0;ids.clear();hold=signatureFailure=networkFailure=false;}
};
}
class UpdateTests : public QObject {
    Q_OBJECT
    QDBusConnection server=QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"),"sovel-update-fixture");
    PackageService service;QTemporaryDir settings;
private slots:
    void initTestCase(){
        qputenv("DBUS_SYSTEM_BUS_ADDRESS",qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        QVERIFY(server.registerService(Service));QVERIFY(server.registerVirtualObject("/",&service,QDBusConnection::SubPath));
    }
    void init(){service.reset();QSettings preferences("Sovel","Desktop");preferences.clear();preferences.sync();}
    void automaticChecksPersistAndRespectDailySchedule(){
        Updates updates;updates.setAutomaticChecks(false);Updates other;QVERIFY(!other.automaticChecks());
        updates.pollAutomaticChecks();QCOMPARE(service.refreshes,0);
        updates.setAutomaticChecks(true);updates.pollAutomaticChecks();
        QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);QCOMPARE(service.refreshes,1);
        QVERIFY(service.hints.contains("interactive=false"));QVERIFY(service.hints.contains("background=true"));
        updates.pollAutomaticChecks();QTest::qWait(100);QCOMPARE(service.refreshes,1);
    }
    void trustedInstallAfterSuccessfulCheck(){
        Updates updates;QVERIFY(!updates.canInstall());updates.install();QCOMPARE(service.installs,0);
        updates.check();QVERIFY(updates.busy());updates.check();
        QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);QCOMPARE(service.refreshes,1);QCOMPARE(service.lists,1);
        QCOMPARE(updates.packages().size(),1);QVERIFY(updates.canInstall());QVERIFY(!updates.lastChecked().isEmpty());
        updates.install();QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);
        QCOMPARE(service.installs,1);QCOMPARE(service.flags,qulonglong(Tx::TransactionFlagOnlyTrusted));
        QCOMPARE(service.ids.size(),1);QVERIFY(updates.error().isEmpty());QVERIFY(updates.restartRequired());QVERIFY(!updates.canInstall());
        QSettings preferences("Sovel","Desktop");QVERIFY(QDateTime::fromString(preferences.value("updates/downloaded/0.1.1-1").toString(),Qt::ISODate).isValid());QVERIFY(!preferences.contains("updates/published/0.1.1-1"));
    }
    void signatureFailureNeverRetriesUntrusted(){
        Updates updates;updates.check();QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);QVERIFY(updates.canInstall());
        service.signatureFailure=true;updates.install();QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);
        QVERIFY(updates.error().contains("signature"));QCOMPARE(service.installs,1);QCOMPARE(service.flags,qulonglong(Tx::TransactionFlagOnlyTrusted));QVERIFY(!updates.canInstall());
    }
    void offlineFailureDoesNotEnableInstall(){
        service.networkFailure=true;Updates updates;updates.check();QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);
        QVERIFY(updates.error().contains("internet"));QCOMPARE(service.lists,0);QVERIFY(!updates.canInstall());
    }
    void cancellationFollowsServicePermission(){
        service.hold=true;Updates updates;updates.check();QTRY_VERIFY_WITH_TIMEOUT(updates.canCancel(),10000);
        QCOMPARE(updates.progress(),25);updates.cancel();QTRY_VERIFY_WITH_TIMEOUT(!updates.busy(),10000);
        QCOMPARE(service.cancels,1);QVERIFY(!updates.canInstall());QVERIFY(updates.status().contains("cancelled"));
    }
};
QTEST_GUILESS_MAIN(UpdateTests)
#include "updates.moc"
