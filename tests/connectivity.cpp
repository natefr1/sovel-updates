#include "connectivity.h"
#include <QDBusVirtualObject>
#include <QDBusArgument>
#include <QDBusVariant>
#include <QtTest>
#include <QTemporaryDir>
#include <time.h>

namespace {
const QString NM="org.freedesktop.NetworkManager",Root="/org/freedesktop/NetworkManager";
const QString Wifi=Root+"/Devices/1",AP1=Root+"/AccessPoint/1",AP2=Root+"/AccessPoint/2";
const QString Adapter="/org/bluez/hci0",Props="org.freedesktop.DBus.Properties";
class Radios : public QDBusVirtualObject {
public:
    uint state=30;int activations=0,starts=0,stops=0;
    bool active=false,delayDiscovery=false,delayAccessPoints=false,rejectScan=false,silentScan=false,hiddenAP=false,savedProfile=false;
    int disconnections=0;
    bool allPoints=false,staleFirst=false,online=true;int allPointCalls=0;
    RadioInterfaces captured;
    RadioObjects devices;
    QString introspect(const QString &) const override{return {};}
    bool handleMessage(const QDBusMessage &m,const QDBusConnection &bus) override {
        QVariantList result;
        if(m.interface()==Props&&m.member()=="GetAll"){
            QVariantMap values;auto interface=m.arguments().value(0).toString();
            if(m.path()==Root)values={{"WirelessEnabled",true}};
            else if(m.path()==Wifi&&interface==NM+".Device")values={{"DeviceType",uint(2)},{"Interface","wlan-test"},{"State",state}};
            else if(m.path()==Wifi)values={{"AccessPoints",QVariant::fromValue(silentScan?QList<QDBusObjectPath>{}:QList<QDBusObjectPath>{QDBusObjectPath(AP1),QDBusObjectPath(AP2)})},{"ActiveAccessPoint",QVariant::fromValue(QDBusObjectPath(active?AP1:"/"))}};
            else if(m.path()==AP1||m.path()==AP2)values={{"Ssid",hiddenAP&&m.path()==AP2?QByteArray():QByteArray("Caf\xc3\xa9 & <home>")},{"Frequency",uint(5180)},{"Flags",uint(1)},{"WpaFlags",uint(0x100)},{"RsnFlags",uint(0x100)},{"Strength",uchar(m.path()==AP1?90:30)}};
            if(staleFirst&&m.path()==AP1){struct timespec clock;clock_gettime(CLOCK_BOOTTIME,&clock);values["LastSeen"]=int(clock.tv_sec-300);}
            result<<values;
        }else if(m.member()=="GetAllAccessPoints"){
            ++allPointCalls;if(!allPoints){bus.send(m.createErrorReply("org.freedesktop.DBus.Error.UnknownMethod","Use property fallback"));return true;}
            result<<QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(AP1),QDBusObjectPath(AP2)});
        }else if(m.member()=="GetDevices")result<<QVariant::fromValue(online?QList<QDBusObjectPath>{QDBusObjectPath(Wifi)}:QList<QDBusObjectPath>{});
        else if(m.member()=="AddAndActivateConnection"){
            ++activations;captured=qdbus_cast<RadioInterfaces>(m.arguments().at(0));
            result<<QVariant::fromValue(QDBusObjectPath(Root+"/Settings/1"))<<QVariant::fromValue(QDBusObjectPath(Root+"/ActiveConnection/1"));
        }else if(m.member()=="ListConnections")result<<QVariant::fromValue(savedProfile?QList<QDBusObjectPath>{QDBusObjectPath(Root+"/Settings/1")}:QList<QDBusObjectPath>{});
        else if(m.member()=="GetSettings")result<<QVariant::fromValue(RadioInterfaces{{"connection",{{"id","Saved café"},{"type","802-11-wireless"},{"uuid","fixture"},{"autoconnect",true}}},{"802-11-wireless",{{"ssid",QByteArray("Caf\xc3\xa9 & <home>")}}}});
        else if(m.member()=="Delete")savedProfile=false;
        else if(m.member()=="ActivateConnection"){++activations;result<<QVariant::fromValue(QDBusObjectPath(Root+"/ActiveConnection/1"));}
        else if(m.member()=="Disconnect"){
            ++disconnections;active=false;state=30;
            auto signal=QDBusMessage::createSignal(Wifi,Props,"PropertiesChanged");signal<<NM+".Device"<<QVariantMap{{"State",state}}<<QStringList{};bus.send(signal);
        }else if(m.member()=="GetManagedObjects"){
            RadioObjects objects;objects[QDBusObjectPath(Adapter)]={{"org.bluez.Adapter1",{{"Powered",true}}}};
            for(auto it=devices.begin();it!=devices.end();++it)objects[it.key()]=it.value();
            result<<QVariant::fromValue(objects);
        }else if(m.interface()==Props&&m.member()=="Set"&&m.arguments().value(1).toString()=="Alias"){
            devices[QDBusObjectPath(m.path())]["org.bluez.Device1"]["Alias"]=m.arguments().value(2).value<QDBusVariant>().variant();
        }else if(m.member()=="StartDiscovery"){
            ++starts;
            if(delayDiscovery){QTimer::singleShot(100,this,[m,bus]{bus.send(m.createReply());});return true;}
        }else if(m.member()=="StopDiscovery")++stops;
        else if(m.member()=="RequestScan"){
            if(silentScan)return true;
            if(rejectScan){bus.send(m.createErrorReply("org.freedesktop.NetworkManager.Device.NotAllowed","The adapter is not available for scanning."));return true;}
            QTimer::singleShot(20,this,[bus]{auto signal=QDBusMessage::createSignal(Wifi,Props,"PropertiesChanged");signal<<NM+".Device.Wireless"<<QVariantMap{{"LastScan",qlonglong(100)}}<<QStringList{};bus.send(signal);});
        }else return false;
        if(delayAccessPoints&&(m.path()==AP1||m.path()==AP2)){
            QTimer::singleShot(1500,this,[m,bus,result]{bus.send(m.createReply(result));});return true;
        }
        bus.send(m.createReply(result));return true;
    }
    void reset(){state=30;activations=starts=stops=disconnections=allPointCalls=0;active=delayDiscovery=delayAccessPoints=rejectScan=silentScan=hiddenAP=savedProfile=allPoints=staleFirst=false;online=true;captured.clear();devices.clear();}
};
}
class ConnectivityTests : public QObject {
    Q_OBJECT
    QDBusConnection server=QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"),"sovel-radio-fixture");
    Radios radios;
    QTemporaryDir settings;
private slots:
    void initTestCase(){
        qputenv("XDG_CONFIG_HOME",settings.path().toUtf8());
        QVERIFY(server.isConnected());QVERIFY(server.registerService(NM));QVERIFY(server.registerService("org.bluez"));
        QVERIFY(server.registerVirtualObject("/",&radios,QDBusConnection::SubPath));
    }
    void init(){radios.reset();QSettings names("Sovel","Desktop");names.clear();names.sync();}
    void allAccessPointsAndStaleResults(){
        radios.allPoints=true;radios.staleFirst=true;Connectivity radio(QDBusConnection::sessionBus(),nullptr);radio.startWifi();
        QTRY_VERIFY_WITH_TIMEOUT(!radio.wifiBusy(),5000);QVERIFY(radios.allPointCalls>0);QCOMPARE(radio.wifiNetworks().size(),1);QCOMPARE(radio.wifiNetworks().first().toMap()["path"].toString(),AP2);radio.stopWifi();
    }
    void adapterRemovalAndResume(){
        Connectivity radio(QDBusConnection::sessionBus(),nullptr);radio.startWifi();QTRY_VERIFY_WITH_TIMEOUT(radio.wifiAvailable()&&!radio.wifiBusy(),5000);
        radios.online=false;auto signal=QDBusMessage::createSignal(Root,NM,"DeviceRemoved");signal<<QVariant::fromValue(QDBusObjectPath(Wifi));server.send(signal);
        QTRY_VERIFY_WITH_TIMEOUT(!radio.wifiAvailable(),5000);QVERIFY(radio.wifiNetworks().isEmpty());
        QVERIFY(QMetaObject::invokeMethod(&radio,"prepareForSleep",Q_ARG(bool,true)));QVERIFY(!radio.wifiBusy());radios.online=true;
        QVERIFY(QMetaObject::invokeMethod(&radio,"prepareForSleep",Q_ARG(bool,false)));QTRY_VERIFY_WITH_TIMEOUT(radio.wifiAvailable()&&!radio.wifiBusy(),5000);radio.stopWifi();
    }
    void advertisedNamesBeatAddressAliasesAndExposeBattery(){
        const auto object=QDBusObjectPath(Adapter+"/dev_A4_C1_38_7B_22_F0");
        radios.devices[object]={{"org.bluez.Device1",{{"Alias","A4:C1:38:7B:22:F0"},{"Name","Nathan's Headphones"},{"Address","A4:C1:38:7B:22:F0"},{"Icon","audio-headset"},{"Paired",true},{"Connected",true}}},{"org.bluez.Battery1",{{"Percentage",uchar(87)}}}};
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startBluetooth();QTRY_COMPARE(controller.bluetoothDevices().size(),1);auto row=controller.bluetoothDevices().first().toMap();QCOMPARE(row["name"].toString(),QString("Nathan's Headphones"));QCOMPARE(row["battery"].toInt(),87);QCOMPARE(row["category"].toString(),QString("Headphones"));QVERIFY(row["connected"].toBool());
    }
    void customNamePersistsWhenTheAdvertisedNameChanges(){
        const auto object=QDBusObjectPath(Adapter+"/dev_A4_C1_38_7B_22_F0");
        radios.devices[object]={{"org.bluez.Device1",{{"Alias","A4:C1:38:7B:22:F0"},{"Name","Headset"},{"Address","A4:C1:38:7B:22:F0"},{"Icon","audio-headset"},{"Paired",true}}}};
        {Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startBluetooth();QTRY_COMPARE(controller.bluetoothDevices().size(),1);controller.renameBluetooth(object.path(),"Studio headphones");QTRY_COMPARE(controller.bluetoothDevices().first().toMap()["name"].toString(),QString("Studio headphones"));QCOMPARE(radios.devices[object]["org.bluez.Device1"]["Alias"].toString(),QString("Studio headphones"));}
        radios.devices[object]["org.bluez.Device1"]["Alias"]="A4:C1:38:7B:22:F0";radios.devices[object]["org.bluez.Device1"]["Name"]="Different advertised name";
        Connectivity reader(QDBusConnection::sessionBus(),nullptr);reader.startBluetooth();QTRY_COMPARE(reader.bluetoothDevices().size(),1);QCOMPARE(reader.bluetoothDevices().first().toMap()["name"].toString(),QString("Studio headphones"));
    }
    void scanDeduplicatesAndPreservesNames(){
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();
        QTRY_COMPARE_WITH_TIMEOUT(controller.wifiNetworks().size(),1,10000);
        auto point=controller.wifiNetworks().first().toMap();
        QCOMPARE(point.value("ssid").toString(),QString::fromUtf8("Caf\xc3\xa9 & <home>"));
        QCOMPARE(point.value("path").toString(),AP1);QCOMPARE(point.value("strength").toInt(),90);
        QVERIFY(!point.contains("ssidBytes"));QVERIFY(!point.contains("devicePath"));
        QVERIFY(!point.value("connected").toBool());
    }
    void credentialsUsePrivateDBusAndWaitForActivation(){
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();
        QTRY_COMPARE_WITH_TIMEOUT(controller.wifiNetworks().size(),1,10000);
        QTRY_VERIFY(!controller.wifiBusy());
        controller.connectWifi(AP1,"short");QTest::qWait(30);QCOMPARE(radios.activations,0);
        const QString password="quoted ' $() password";
        controller.connectWifi(AP1,password);
        QTRY_COMPARE_WITH_TIMEOUT(radios.activations,1,10000);
        QCOMPARE(radios.captured["802-11-wireless-security"]["psk"].toString(),password);
        QCOMPARE(radios.captured["802-11-wireless"]["ssid"].toByteArray(),QByteArray("Caf\xc3\xa9 & <home>"));
        QVERIFY(controller.wifiBusy());QVERIFY(!controller.wifiStatus().contains(password));
        // An accepted request is not a working connection. Only State=100 is.
        radios.active=true;radios.state=40;
        auto change=QDBusMessage::createSignal(Wifi,Props,"PropertiesChanged");
        change<<NM+".Device"<<QVariantMap{{"State",uint(40)}}<<QStringList{};server.send(change);
        QTest::qWait(900);QVERIFY(controller.wifiBusy());
        radios.state=100;server.send(change);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wifiBusy(),10000);
        QVERIFY(controller.wifiNetworks().first().toMap().value("connected").toBool());
    }
    void continuousWifiSignalsDoNotStarveEnumeration(){
        radios.delayAccessPoints=true;
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);
        QTimer noise;noise.setInterval(10);
        connect(&noise,&QTimer::timeout,this,[this]{auto signal=QDBusMessage::createSignal(AP1,Props,"PropertiesChanged");signal<<NM+".AccessPoint"<<QVariantMap{{"Strength",uchar(90)}}<<QStringList{};server.send(signal);});
        noise.start();controller.startWifi();
        QTRY_COMPARE_WITH_TIMEOUT(controller.wifiNetworks().size(),1,5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wifiBusy(),5000);
        noise.stop();controller.stopWifi();
    }
    void scanErrorIsVisibleAndStopsBusyState(){
        radios.rejectScan=true;Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();
        QTRY_VERIFY_WITH_TIMEOUT(controller.wifiStatus().contains("not available for scanning"),5000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wifiBusy(),5000);
    }
    void unresponsiveScanHasBoundedVisibleFailure(){
        radios.silentScan=true;Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();
        QTRY_VERIFY_WITH_TIMEOUT(controller.wifiStatus().contains("No networks returned"),18000);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.wifiBusy(),3000);
        controller.stopWifi();
    }
    void failedAuthenticationIsVisible(){
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();
        QTRY_COMPARE_WITH_TIMEOUT(controller.wifiNetworks().size(),1,10000);QTRY_VERIFY(!controller.wifiBusy());
        controller.connectWifi(AP1,"wrong-password");QTRY_COMPARE(radios.activations,1);
        auto failed=QDBusMessage::createSignal(Wifi,NM+".Device","StateChanged");failed<<uint(120)<<uint(60)<<uint(7);server.send(failed);
        QTRY_VERIFY_WITH_TIMEOUT(controller.wifiStatus().contains("authenticate"),10000);
        QVERIFY(!controller.wifiBusy());QVERIFY(!controller.wifiStatus().contains("wrong-password"));
    }
    void savedNetworksReconnectDisconnectAndForget(){
        radios.savedProfile=true;Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();
        QTRY_COMPARE(controller.savedWifi().size(),1);QTRY_COMPARE(controller.wifiNetworks().size(),1);QTRY_VERIFY(!controller.wifiBusy());
        const auto saved=controller.savedWifi().first().toMap();QVERIFY(!saved.contains("ssidBytes"));QVERIFY(!saved.contains("password"));
        controller.connectSavedWifi(saved["path"].toString());QTRY_COMPARE(radios.activations,1);QVERIFY(controller.wifiBusy());
        radios.active=true;radios.state=100;auto signal=QDBusMessage::createSignal(Wifi,Props,"PropertiesChanged");signal<<NM+".Device"<<QVariantMap{{"State",uint(100)}}<<QStringList{};server.send(signal);
        QTRY_VERIFY(!controller.wifiBusy());QVERIFY(controller.wifiNetworks().first().toMap()["connected"].toBool());
        controller.disconnectWifi(AP1);QTRY_COMPARE(radios.disconnections,1);QTRY_VERIFY(!controller.wifiNetworks().first().toMap()["connected"].toBool());
        controller.forgetWifi(saved["path"].toString());QTRY_VERIFY(controller.savedWifi().isEmpty());QVERIFY(!radios.savedProfile);
    }
    void hiddenNetworksAndBandMetadata(){
        radios.hiddenAP=true;Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startWifi();QTRY_COMPARE(controller.wifiNetworks().size(),2);QTRY_VERIFY(!controller.wifiBusy());
        QCOMPARE(controller.wifiNetworks().first().toMap()["frequency"].toUInt(),uint(5180));QVERIFY(controller.wifiNetworks().last().toMap()["hidden"].toBool());
        controller.connectHiddenWifi(QString(33,'x'),"password123","wpa-psk");QCOMPARE(radios.activations,0);
        controller.connectHiddenWifi("Private café","password123","wpa-psk");QTRY_COMPARE(radios.activations,1);
        QVERIFY(radios.captured["802-11-wireless"]["hidden"].toBool());QCOMPARE(radios.captured["802-11-wireless"]["ssid"].toByteArray(),QString("Private café").toUtf8());QVERIFY(!controller.wifiStatus().contains("password123"));
    }
    void bluetoothDiscoveryStopsWithPage(){
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startBluetooth();
        QTRY_COMPARE_WITH_TIMEOUT(radios.starts,1,10000);controller.stopBluetooth();
        QTRY_COMPARE_WITH_TIMEOUT(radios.stops,1,10000);
    }
    void lateBluetoothDiscoveryAlsoStops(){
        radios.delayDiscovery=true;
        Connectivity controller(QDBusConnection::sessionBus(),nullptr);controller.startBluetooth();
        QTRY_COMPARE_WITH_TIMEOUT(radios.starts,1,10000);controller.stopBluetooth();
        QTRY_COMPARE_WITH_TIMEOUT(radios.stops,1,10000);
    }
};
QTEST_GUILESS_MAIN(ConnectivityTests)
#include "connectivity.moc"
