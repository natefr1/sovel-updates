#include "connectivity.h"
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusVariant>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>
#include <memory>
#include <unistd.h>
#include <QFile>
#include <time.h>

namespace {
const QString NM="org.freedesktop.NetworkManager",NMRoot="/org/freedesktop/NetworkManager";
const QString Device=NM+".Device",Wireless=Device+".Wireless",AP=NM+".AccessPoint";
const QString Props="org.freedesktop.DBus.Properties",Bluez="org.bluez";
QVariantMap properties(const QDBusMessage &reply){return reply.arguments().isEmpty()?QVariantMap():qdbus_cast<QVariantMap>(reply.arguments().first());}
QString path(const QVariant &value){return value.value<QDBusObjectPath>().path();}
QString errorText(const QDBusMessage &reply,const QString &fallback){
    if(reply.errorName().contains("AccessDenied")||reply.errorName().contains("NotAuthorized"))return "Authorization is required. Check the system authentication dialog.";
    if(reply.errorName().contains("ServiceUnknown")||reply.errorName().contains("NameHasNoOwner"))return fallback;
    return reply.errorMessage().isEmpty()?fallback:reply.errorMessage();
}
QString deviceCategory(const QVariantMap &device){
    const auto icon=device.value("Icon").toString().toLower();
    if(icon.contains("headset")||icon.contains("headphone"))return "Headphones";
    if(icon.contains("audio"))return "Speaker";
    if(icon.contains("keyboard"))return "Keyboard";
    if(icon.contains("mouse"))return "Mouse";
    if(icon.contains("phone"))return "Phone";
    if(icon.contains("game"))return "Game controller";
    if(icon.contains("computer"))return "Computer";
    const auto major=(device.value("Class").toUInt()>>8)&31;
    if(major==4)return "Audio device";
    if(major==5)return "Input device";
    if(major==2)return "Phone";
    if(major==1)return "Computer";
    return {};
}
QString namedDevice(const QVariantMap &device,const QString &custom){
    if(!custom.trimmed().isEmpty())return custom.trimmed();
    const auto address=device.value("Address").toString();
    auto compact=[](QString text){return text.remove(':').remove('-').toUpper();};
    for(const auto &key:{"Alias","Name"}){
        const auto value=device.value(key).toString().trimmed();
        if(!value.isEmpty()&&compact(value)!=compact(address)&&!QRegularExpression("^[0-9A-Fa-f]{2}([:-][0-9A-Fa-f]{2}){5}$").match(value).hasMatch())return value;
    }
    const auto category=deviceCategory(device);
    return !category.isEmpty()?category:!address.isEmpty()?address:QString("Bluetooth device");
}
}

Connectivity::Connectivity(QObject *parent):Connectivity(QDBusConnection::systemBus(),parent){}
Connectivity::Connectivity(const QDBusConnection &bus,QObject *parent):QObject(parent),m_bus(bus){
    qDBusRegisterMetaType<RadioInterfaces>();qDBusRegisterMetaType<RadioObjects>();
    m_wifiTimer.setInterval(20000);
    m_wifiDebounce.setSingleShot(true);m_wifiDebounce.setInterval(600);
    m_bluetoothDebounce.setSingleShot(true);m_bluetoothDebounce.setInterval(500);
    m_wifiConnectTimeout.setSingleShot(true);m_wifiConnectTimeout.setInterval(60000);
    m_scanTimeout.setSingleShot(true);m_scanTimeout.setInterval(12000);
    m_wifiRoundTimeout.setSingleShot(true);m_wifiRoundTimeout.setInterval(12000);
    connect(&m_scanTimeout,&QTimer::timeout,this,[this]{
        ++m_scanGeneration;m_scanPending=false;m_scanningDevices.clear();
        if(m_networks.isEmpty())m_wifiError="No networks returned by the adapter. Try Scan again, or unplug and reconnect the USB Wi-Fi adapter.";
        refreshWifi(false);
    });
    connect(&m_wifiRoundTimeout,&QTimer::timeout,this,[this]{
        ++m_wifiGeneration;m_wifiBusy=false;m_scanPending=false;m_scanTimeout.stop();
        m_wifiStatus="The Wi-Fi service did not respond. Try Scan again.";emit wifiChanged();
    });
    connect(&m_wifiConnectTimeout,&QTimer::timeout,this,[this]{
        m_wifiConnecting=false;m_pendingWifi.clear();m_wifiError="The connection timed out. Check the password and try again.";
        m_wifiStatus=m_wifiError;emit wifiChanged();
    });
    // NetworkManager schedules its own background scans. Poll cached results;
    // retry a failed hardware scan only when the user requests it.
    connect(&m_wifiTimer,&QTimer::timeout,this,[this]{if(!wifiBusy())refreshWifi(true);});
    connect(&m_wifiDebounce,&QTimer::timeout,this,[this]{refreshWifi(false);});
    connect(&m_bluetoothDebounce,&QTimer::timeout,this,[this]{refreshBluetooth();});
    m_bus.connect(NM,QString(),Props,"PropertiesChanged",this,SLOT(propertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    m_bus.connect(NM,QString(),Device,"StateChanged",this,SLOT(deviceStateChanged(uint,uint,uint,QDBusMessage)));
    m_bus.connect(NM,NMRoot,NM,"DeviceAdded",this,SLOT(networkDevicesChanged(QDBusObjectPath)));
    m_bus.connect(NM,NMRoot,NM,"DeviceRemoved",this,SLOT(networkDevicesChanged(QDBusObjectPath)));
    m_bus.connect("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager","PrepareForSleep",this,SLOT(prepareForSleep(bool)));
    m_bus.connect(Bluez,QString(),Props,"PropertiesChanged",this,SLOT(propertiesChanged(QString,QVariantMap,QStringList,QDBusMessage)));
    m_bus.connect(Bluez,"/","org.freedesktop.DBus.ObjectManager","InterfacesAdded",this,SLOT(bluezAdded(QDBusObjectPath,RadioInterfaces)));
    m_bus.connect(Bluez,"/","org.freedesktop.DBus.ObjectManager","InterfacesRemoved",this,SLOT(bluezRemoved(QDBusObjectPath,QStringList)));
}
Connectivity::~Connectivity(){stopBluetooth();}
void Connectivity::call(const QString &service,const QString &object,const QString &interface,const QString &method,const QVariantList &arguments,Reply reply){
    auto message=QDBusMessage::createMethodCall(service,object,interface,method);message.setArguments(arguments);
    auto *watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(message,45000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[watcher,reply=std::move(reply)]{reply(watcher->reply());watcher->deleteLater();});
}
void Connectivity::startWifi(){m_wifiWatching=true;m_wifiTimer.start();refreshSavedWifi();scanWifi();}
void Connectivity::stopWifi(){m_wifiWatching=false;m_wifiTimer.stop();m_wifiDebounce.stop();m_scanTimeout.stop();++m_scanGeneration;m_scanPending=false;m_scanningDevices.clear();}
void Connectivity::scanWifi(){if(!wifiBusy()){m_wifiError.clear();refreshSavedWifi();refreshWifi(true);}}
void Connectivity::refreshWifi(bool requestScan){
    // Property signals can arrive continuously during a hardware scan. Never
    // invalidate an in-flight enumeration on each signal: it may never finish.
    if(m_wifiBusy){m_wifiRefreshAgain=true;return;}
    const auto generation=++m_wifiGeneration;
    if(requestScan){++m_scanGeneration;m_scanningDevices.clear();}
    m_wifiBusy=true;m_wifiRoundTimeout.start();emit wifiChanged();
    call(NM,NMRoot,Props,"GetAll",{NM},[this,generation,requestScan](const QDBusMessage &managerReply){
        if(generation!=m_wifiGeneration)return;
        if(managerReply.type()==QDBusMessage::ErrorMessage){m_wifiRoundTimeout.stop();m_wifiAvailable=false;m_wifiBusy=false;m_networks.clear();m_wifiStatus="The Wi-Fi service is unavailable.";emit wifiChanged();return;}
        m_wifiEnabled=properties(managerReply).value("WirelessEnabled").toBool();
        if(properties(managerReply).contains("WirelessHardwareEnabled")&&!properties(managerReply).value("WirelessHardwareEnabled").toBool())m_wifiError="Wi-Fi is blocked by the hardware switch or airplane mode.";
        struct Round{int pending=1,adapters=0;QMap<QString,QVariantMap> points;QStringList devices;};
        auto round=std::make_shared<Round>();
        auto done=[this,generation,round]{
            if(--round->pending!=0||generation!=m_wifiGeneration)return;
            m_wifiAvailable=round->adapters>0;m_wifiDevices=round->devices;m_accessPoints=round->points;m_networks.clear();
            QMap<QString,QVariantMap> best;
            for(const auto &point:round->points){
                const QString key=(point.value("hidden").toBool()?point.value("path").toString():point.value("ssid").toString())+QChar(0)+point.value("security").toString();
                if(!best.contains(key)||point.value("connected").toBool()||(!best[key].value("connected").toBool()&&point.value("strength").toInt()>best[key].value("strength").toInt()))best[key]=point;
            }
            for(auto point:best){point.remove("ssidBytes");point.remove("devicePath");m_networks.append(point);}
            std::sort(m_networks.begin(),m_networks.end(),[](const QVariant &a,const QVariant &b){auto x=a.toMap(),y=b.toMap();if(x.value("connected")!=y.value("connected"))return x.value("connected").toBool();return x.value("strength").toInt()>y.value("strength").toInt();});
            m_wifiBusy=false;
            m_wifiRoundTimeout.stop();
            if(m_wifiConnecting&&std::any_of(m_accessPoints.cbegin(),m_accessPoints.cend(),[this](const QVariantMap &ap){return ap.value("connected").toBool()&&ap.value("devicePath").toString()==m_pendingWifiDevice&&(m_pendingWifiSsid.isEmpty()||ap.value("ssid").toString()==m_pendingWifiSsid);})){
                m_wifiConnecting=false;m_wifiConnectTimeout.stop();m_wifiError.clear();
            }
            if(!m_wifiConnecting)m_wifiStatus=!m_wifiError.isEmpty()?m_wifiError:!m_wifiAvailable?"No Wi-Fi adapter detected. Connect a supported adapter or use Ethernet.":!m_wifiEnabled?"Wi-Fi is off.":m_scanPending?"Scanning nearby Wi-Fi networks…":m_networks.isEmpty()?"No nearby networks found. Try Scan again.":"Choose a nearby network to connect.";
            emit wifiChanged();
            if(m_wifiRefreshAgain){m_wifiRefreshAgain=false;if(!m_wifiDebounce.isActive())m_wifiDebounce.start();}
        };
        call(NM,NMRoot,NM,"GetDevices",{},[this,generation,requestScan,round,done](const QDBusMessage &reply){
            if(generation!=m_wifiGeneration)return;
            if(reply.type()!=QDBusMessage::ErrorMessage&&!reply.arguments().isEmpty())for(const auto &object:qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().first())){
                ++round->pending;
                call(NM,object.path(),Props,"GetAll",{Device},[this,generation,requestScan,round,done,object](const QDBusMessage &deviceReply){
                    if(generation!=m_wifiGeneration)return;
                    auto device=properties(deviceReply);
                    if(device.value("DeviceType").toUInt()!=2){done();return;}
                    ++round->adapters;round->devices<<object.path();
                    if(device.value("Managed").isValid()&&!device.value("Managed").toBool())m_wifiError="The Wi-Fi adapter is not managed by the network service.";
                    if(requestScan&&m_wifiEnabled){
                        const auto scanGeneration=m_scanGeneration;m_scanningDevices.insert(object.path());
                        m_scanPending=true;m_scanTimeout.start();
                        call(NM,object.path(),Wireless,"RequestScan",{QVariantMap()},[this,scanGeneration,object](const QDBusMessage &reply){
                            if(scanGeneration!=m_scanGeneration)return;
                            if(reply.type()==QDBusMessage::ErrorMessage){m_scanningDevices.remove(object.path());m_scanPending=!m_scanningDevices.isEmpty();if(!m_scanPending)m_scanTimeout.stop();m_wifiError=errorText(reply,"The adapter could not scan for networks.");}
                            if(m_wifiWatching&&!m_wifiDebounce.isActive())m_wifiDebounce.start();
                        });
                    }
                    ++round->pending;
                    call(NM,object.path(),Props,"GetAll",{Wireless},[this,generation,round,done,object,device](const QDBusMessage &wirelessReply){
                        if(generation!=m_wifiGeneration)return;
                        const auto wireless=properties(wirelessReply);const QString active=path(wireless.value("ActiveAccessPoint"));
                        auto enumerate=[this,generation,round,done,object,device,wireless,active](const QList<QDBusObjectPath> &points){
                        for(const auto &access:points){
                            ++round->pending;
                            call(NM,access.path(),Props,"GetAll",{AP},[this,generation,round,done,object,device,access,active,wirelessBitrate=wireless.value("Bitrate").toUInt()](const QDBusMessage &apReply){
                                if(generation!=m_wifiGeneration)return;
                                const auto ap=properties(apReply);const auto bytes=ap.value("Ssid").toByteArray();
                                if(apReply.type()!=QDBusMessage::ErrorMessage){
                                    struct timespec clock;clock_gettime(CLOCK_BOOTTIME,&clock);const auto lastSeen=ap.value("LastSeen",-1).toLongLong();
                                    if(access.path()!=active&&lastSeen>=0&&clock.tv_sec-lastSeen>120){done();return;}
                                    const uint security=ap.value("WpaFlags").toUInt()|ap.value("RsnFlags").toUInt();
                                    const bool locked=ap.value("Flags").toUInt()&1,enterprise=security&(0x200|0x2000),owe=security&(0x800|0x1000);
                                    const QString kind=enterprise?"Enterprise":owe?"Enhanced open":security&0x100?"WPA/WPA2":security&0x400?"WPA3":locked?"Legacy security":"Open";
                                    round->points[access.path()]={{"path",access.path()},{"ssid",bytes.isEmpty()?QString("Hidden network"):QString::fromUtf8(bytes)},{"hidden",bytes.isEmpty()},{"ssidBytes",bytes},{"devicePath",object.path()},{"device",device.value("Interface")},{"security",kind},{"keyManagement",owe?"owe":security&0x100?"wpa-psk":security&0x400?"sae":""},{"needsPassword",locked&&!owe},{"advanced",enterprise||(locked&&!(security&(0x100|0x400|0x800|0x1000)))},{"frequency",ap.value("Frequency").toUInt()},{"speed",wirelessBitrate},{"mac",device.value("HwAddress")},{"strength",ap.value("Strength").toUInt()},{"connected",access.path()==active&&device.value("State").toUInt()==100}};
                                }
                                done();
                            });
                        }
                        done();
                        };
                        call(NM,object.path(),Wireless,"GetAllAccessPoints",{},[enumerate,wireless](const QDBusMessage &reply){
                            enumerate(reply.type()==QDBusMessage::ErrorMessage?qdbus_cast<QList<QDBusObjectPath>>(wireless.value("AccessPoints")):qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().value(0)));
                        });
                    });
                    done();
                });
            }
            done();
        });
    });
}
void Connectivity::setWifiEnabled(bool enabled){
    m_wifiError.clear();
    if(!enabled){m_wifiConnecting=false;m_wifiConnectTimeout.stop();}
    call(NM,NMRoot,Props,"Set",{NM,"WirelessEnabled",QVariant::fromValue(QDBusVariant(enabled))},[this](const QDBusMessage &reply){
        if(reply.type()==QDBusMessage::ErrorMessage){m_wifiStatus=errorText(reply,"Wi-Fi could not be changed.");emit wifiChanged();}else refreshWifi(true);
    });
}
void Connectivity::connectWifi(const QString &accessPoint,const QString &password){
    if(!m_accessPoints.contains(accessPoint)||wifiBusy())return;
    const auto ap=m_accessPoints.value(accessPoint);
    if(ap.value("advanced").toBool()){m_wifiStatus="This network requires advanced security settings. Use Advanced Connections.";emit wifiChanged();return;}
    if(ap.value("needsPassword").toBool()&&(password.toUtf8().size()<8||password.toUtf8().size()>63)&&!(password.size()==64&&QRegularExpression("^[A-Fa-f0-9]{64}$").match(password).hasMatch())){m_wifiStatus="Enter the network password (8–64 characters).";emit wifiChanged();return;}
    RadioInterfaces connection;
    QVariantMap identity{{"id",ap.value("ssid")},{"uuid",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"type","802-11-wireless"},{"autoconnect",true}};
    if(geteuid()!=0)identity["permissions"]=QStringList{"user:"+qEnvironmentVariable("USER")+":"};
    connection["connection"]=identity;
    connection["802-11-wireless"]={{"ssid",ap.value("ssidBytes")},{"mode","infrastructure"}};
    const auto key=ap.value("keyManagement").toString();
    if(!key.isEmpty()){
        connection["802-11-wireless"]["security"]="802-11-wireless-security";
        connection["802-11-wireless-security"]={{"key-mgmt",key}};
        if(ap.value("needsPassword").toBool())connection["802-11-wireless-security"]["psk"]=password;
    }
    connection["ipv4"]={{"method","auto"}};connection["ipv6"]={{"method","auto"}};
    activateWifi(connection,ap.value("devicePath").toString(),accessPoint,ap.value("ssid").toString(),ap.value("ssid").toString());
}

void Connectivity::startBluetooth(){m_bluetoothWatching=true;refreshBluetooth(true);}
void Connectivity::scanBluetooth(){refreshBluetooth(true);}
void Connectivity::stopBluetooth(){
    m_bluetoothWatching=false;m_bluetoothDebounce.stop();
    for(const auto &adapter:m_discoveryLeases){auto message=QDBusMessage::createMethodCall(Bluez,adapter,"org.bluez.Adapter1","StopDiscovery");m_bus.call(message,QDBus::NoBlock);}
    m_discoveryLeases.clear();
}
void Connectivity::refreshBluetooth(bool discover){
    call(Bluez,"/","org.freedesktop.DBus.ObjectManager","GetManagedObjects",{},[this,discover](const QDBusMessage &reply){
        if(reply.type()==QDBusMessage::ErrorMessage){m_adapters.clear();m_devices.clear();m_bluetoothStatus="Bluetooth is unavailable. Check the adapter and Bluetooth service.";emit bluetoothChanged();return;}
        const auto objects=qdbus_cast<RadioObjects>(reply.arguments().value(0));
        m_adapters.clear();m_devices.clear();m_bluetoothPaths.clear();m_bluetoothEnabled=false;
        for(auto it=objects.cbegin();it!=objects.cend();++it)if(it.value().contains("org.bluez.Adapter1")){
            const auto adapter=it.value().value("org.bluez.Adapter1");const auto name=it.key().path();m_adapters<<name;m_bluetoothEnabled|=adapter.value("Powered").toBool();
            if(discover&&m_bluetoothWatching&&adapter.value("Powered").toBool()&&!m_discoveryLeases.contains(name)&&!m_startingDiscovery.contains(name)){
                m_startingDiscovery.insert(name);
                call(Bluez,name,"org.bluez.Adapter1","StartDiscovery",{},[this,name](const QDBusMessage &result){
                    m_startingDiscovery.remove(name);
                    if(result.type()!=QDBusMessage::ErrorMessage){
                        if(m_bluetoothWatching)m_discoveryLeases<<name;
                        else {auto stop=QDBusMessage::createMethodCall(Bluez,name,"org.bluez.Adapter1","StopDiscovery");m_bus.call(stop,QDBus::NoBlock);}
                    }
                    else {m_bluetoothStatus=errorText(result,"Nearby devices could not be scanned.");emit bluetoothChanged();}
                });
            }
        }
        for(auto it=objects.cbegin();it!=objects.cend();++it)if(it.value().contains("org.bluez.Device1")){
            auto device=it.value().value("org.bluez.Device1");
            const auto address=device.value("Address").toString().toUpper();m_names.sync();
            const auto name=namedDevice(device,m_names.value("bluetooth/names/"+address).toString());
            const auto battery=it.value().value("org.bluez.Battery1").value("Percentage");
            QVariantMap row{{"path",it.key().path()},{"name",name},{"address",address},{"advertisedName",device.value("Name")},{"category",deviceCategory(device)},{"battery",battery.isValid()?qBound(0,battery.toInt(),100):-1},{"adapter",path(device.value("Adapter"))},{"paired",device.value("Paired")},{"connected",device.value("Connected")},{"icon",device.value("Icon","bluetooth")}};
            m_devices<<row;m_bluetoothPaths[it.key().path()]=row;
        }
        std::sort(m_devices.begin(),m_devices.end(),[](const QVariant &a,const QVariant &b){auto x=a.toMap(),y=b.toMap();if(x.value("connected")!=y.value("connected"))return x.value("connected").toBool();if(x.value("paired")!=y.value("paired"))return x.value("paired").toBool();return x.value("name").toString().localeAwareCompare(y.value("name").toString())<0;});
        if(!m_bluetoothBusy)m_bluetoothStatus=m_adapters.isEmpty()?"No Bluetooth adapter detected. A driver or USB Bluetooth adapter may be needed.":!m_bluetoothEnabled?"Bluetooth is off.":"Nearby devices appear automatically. Put your device into pairing mode.";
        emit bluetoothChanged();
    });
}
void Connectivity::setBluezProperty(const QString &object,const QString &interface,const QString &name,const QVariant &value,Reply reply){call(Bluez,object,Props,"Set",{interface,name,QVariant::fromValue(QDBusVariant(value))},std::move(reply));}
void Connectivity::renameBluetooth(const QString &object,const QString &name){
    if(!m_bluetoothPaths.contains(object)||m_bluetoothBusy||!m_bluetoothPaths[object].value("paired").toBool())return;
    const auto clean=name.trimmed();
    if(clean.size()>80||QRegularExpression("[\\x{0000}-\\x{001f}\\x{007f}]").match(clean).hasMatch()){
        m_bluetoothStatus="Use a device name of up to 80 characters without control characters.";emit bluetoothChanged();return;
    }
    const auto address=m_bluetoothPaths[object].value("address").toString();m_bluetoothBusy=true;emit bluetoothChanged();
    setBluezProperty(object,"org.bluez.Device1","Alias",clean,[this,address,clean](const QDBusMessage &reply){
        m_bluetoothBusy=false;
        if(reply.type()==QDBusMessage::ErrorMessage){m_bluetoothStatus=errorText(reply,"The device could not be renamed.");emit bluetoothChanged();return;}
        if(clean.isEmpty())m_names.remove("bluetooth/names/"+address);else m_names.setValue("bluetooth/names/"+address,clean);
        m_names.sync();refreshBluetooth();
    });
}
void Connectivity::setBluetoothEnabled(bool enabled){
    for(const auto &adapter:m_adapters)setBluezProperty(adapter,"org.bluez.Adapter1","Powered",enabled,[this](const QDBusMessage &reply){
        if(reply.type()==QDBusMessage::ErrorMessage){m_bluetoothStatus=errorText(reply,"Bluetooth is blocked or unavailable. Check airplane mode and the hardware switch.");emit bluetoothChanged();}else refreshBluetooth(true);
    });
}
void Connectivity::connectBluetooth(const QString &device){
    if(!m_bluetoothPaths.contains(device)||m_bluetoothBusy)return;
    m_bluetoothBusy=true;m_bluetoothStatus="Connecting to "+m_bluetoothPaths[device].value("name").toString()+"…";emit bluetoothChanged();
    if(m_bluetoothPaths[device].value("paired").toBool()){bluetoothConnect(device);return;}
    m_bluetoothStatus="Confirm any pairing code in the Bluetooth dialog.";emit bluetoothChanged();
    call(Bluez,device,"org.bluez.Device1","Pair",{},[this,device](const QDBusMessage &reply){
        if(reply.type()==QDBusMessage::ErrorMessage){m_bluetoothBusy=false;m_bluetoothStatus=errorText(reply,"Pairing failed. Make sure the device is in pairing mode and try again.");emit bluetoothChanged();return;}
        setBluezProperty(device,"org.bluez.Device1","Trusted",true,[this,device](const QDBusMessage &){bluetoothConnect(device);});
    });
}
void Connectivity::bluetoothConnect(const QString &device){
    call(Bluez,device,"org.bluez.Device1","Connect",{},[this](const QDBusMessage &reply){m_bluetoothBusy=false;if(reply.type()==QDBusMessage::ErrorMessage){m_bluetoothStatus=errorText(reply,"The device could not be connected.");emit bluetoothChanged();}else refreshBluetooth();});
}
void Connectivity::disconnectBluetooth(const QString &device){
    if(!m_bluetoothPaths.contains(device)||m_bluetoothBusy)return;
    m_bluetoothBusy=true;emit bluetoothChanged();
    call(Bluez,device,"org.bluez.Device1","Disconnect",{},[this](const QDBusMessage &reply){m_bluetoothBusy=false;if(reply.type()==QDBusMessage::ErrorMessage){m_bluetoothStatus=errorText(reply,"The device could not be disconnected.");emit bluetoothChanged();}else refreshBluetooth();});
}
void Connectivity::forgetBluetooth(const QString &device){
    if(!m_bluetoothPaths.contains(device)||m_bluetoothBusy)return;
    const auto adapter=m_bluetoothPaths[device].value("adapter").toString();
    m_bluetoothBusy=true;emit bluetoothChanged();
    call(Bluez,adapter,"org.bluez.Adapter1","RemoveDevice",{QVariant::fromValue(QDBusObjectPath(device))},[this](const QDBusMessage &reply){m_bluetoothBusy=false;if(reply.type()==QDBusMessage::ErrorMessage){m_bluetoothStatus=errorText(reply,"The device could not be removed.");emit bluetoothChanged();}else refreshBluetooth();});
}
void Connectivity::propertiesChanged(const QString &interface,const QVariantMap &changes,const QStringList &,const QDBusMessage &message){
    if(interface==Wireless&&changes.contains("LastScan")){m_scanningDevices.remove(message.path());m_scanPending=!m_scanningDevices.isEmpty();if(!m_scanPending)m_scanTimeout.stop();}
    if((m_wifiWatching||m_wifiConnecting)&&interface.startsWith(NM)&&!m_wifiDebounce.isActive())m_wifiDebounce.start();
    if(m_bluetoothWatching&&interface.startsWith(Bluez))m_bluetoothDebounce.start();
}
void Connectivity::deviceStateChanged(uint state,uint,uint reason,const QDBusMessage &message){
    if(m_wifiWatching&&state==100)refreshSavedWifi();
    if(m_wifiConnecting&&message.path()==m_pendingWifiDevice){
        if(state==120){
            m_wifiConnecting=false;m_wifiConnectTimeout.stop();
            m_wifiError=(reason==7||reason==8||reason==9)?"The network could not authenticate. Check the password and try again.":"The network connection failed. Check the signal and try again.";
            m_wifiStatus=m_wifiError;emit wifiChanged();
        }else m_wifiDebounce.start();
    }
}
void Connectivity::bluezAdded(const QDBusObjectPath &,const RadioInterfaces &){if(m_bluetoothWatching)m_bluetoothDebounce.start();}
void Connectivity::bluezRemoved(const QDBusObjectPath &,const QStringList &){if(m_bluetoothWatching)m_bluetoothDebounce.start();}

void Connectivity::refreshSavedWifi(){
 if(m_savedBusy)return;m_savedBusy=true;const auto generation=++m_savedGeneration;
 call(NM,NMRoot+"/Settings",NM+".Settings","ListConnections",{},[this,generation](const QDBusMessage &reply){
  if(generation!=m_savedGeneration)return;
  struct SavedRound{int pending=1;QMap<QString,QVariantMap> profiles;};auto round=std::make_shared<SavedRound>();
  auto done=[this,generation,round]{if(--round->pending||generation!=m_savedGeneration)return;m_savedBusy=false;m_savedProfiles=round->profiles;m_savedNetworks.clear();for(auto profile:round->profiles){profile.remove("ssidBytes");m_savedNetworks<<profile;}std::sort(m_savedNetworks.begin(),m_savedNetworks.end(),[](const QVariant &a,const QVariant &b){return a.toMap()["name"].toString().localeAwareCompare(b.toMap()["name"].toString())<0;});emit wifiChanged();};
  if(reply.type()!=QDBusMessage::ErrorMessage)for(auto object:qdbus_cast<QList<QDBusObjectPath>>(reply.arguments().value(0))){
   ++round->pending;call(NM,object.path(),NM+".Settings.Connection","GetSettings",{},[this,generation,round,done,object](const QDBusMessage &r){
    if(generation!=m_savedGeneration)return;auto settings=qdbus_cast<RadioInterfaces>(r.arguments().value(0));const auto identity=settings.value("connection"),wifi=settings.value("802-11-wireless");
    if(identity.value("type")=="802-11-wireless")round->profiles[object.path()]={{"path",object.path()},{"name",identity.value("id")},{"ssid",QString::fromUtf8(wifi.value("ssid").toByteArray())},{"ssidBytes",wifi.value("ssid")},{"hidden",wifi.value("hidden",false)},{"autoconnect",identity.value("autoconnect",true)},{"uuid",identity.value("uuid")}};
    done();
   });
  }done();
 });
}
void Connectivity::activateWifi(const RadioInterfaces &connection,const QString &device,const QString &specific,const QString &label,const QString &ssid){
 m_wifiError.clear();m_wifiConnecting=true;m_pendingWifi=specific;m_pendingWifiDevice=device;m_pendingWifiSsid=ssid;m_wifiConnectTimeout.start();m_wifiStatus="Connecting to "+label+"…";emit wifiChanged();
 call(NM,NMRoot,NM,"AddAndActivateConnection",{QVariant::fromValue(connection),QVariant::fromValue(QDBusObjectPath(device)),QVariant::fromValue(QDBusObjectPath(specific))},[this](const QDBusMessage &reply){
  if(reply.type()==QDBusMessage::ErrorMessage){m_wifiConnecting=false;m_wifiConnectTimeout.stop();m_wifiError=errorText(reply,"The network could not be connected.");m_wifiStatus=m_wifiError;emit wifiChanged();}
  else {m_wifiStatus="Connection requested. Waiting for the network…";emit wifiChanged();refreshSavedWifi();m_wifiDebounce.start();}
 });
}
void Connectivity::connectSavedWifi(const QString &profile){
 if(wifiBusy()||!m_savedProfiles.contains(profile)||m_wifiDevices.isEmpty())return;
 const auto saved=m_savedProfiles[profile];QString device=m_wifiDevices.first(),specific="/";
 for(auto ap:m_accessPoints)if(ap.value("ssidBytes")==saved.value("ssidBytes")){device=ap.value("devicePath").toString();specific=ap.value("path").toString();break;}
 m_wifiConnecting=true;m_pendingWifi=specific;m_pendingWifiDevice=device;m_pendingWifiSsid=saved.value("ssid").toString();m_wifiConnectTimeout.start();m_wifiError.clear();m_wifiStatus="Connecting to "+saved.value("name").toString()+"…";emit wifiChanged();
 call(NM,NMRoot,NM,"ActivateConnection",{QVariant::fromValue(QDBusObjectPath(profile)),QVariant::fromValue(QDBusObjectPath(device)),QVariant::fromValue(QDBusObjectPath(specific))},[this](const QDBusMessage &r){if(r.type()==QDBusMessage::ErrorMessage){m_wifiConnecting=false;m_wifiConnectTimeout.stop();m_wifiError=errorText(r,"Saved network could not connect.");m_wifiStatus=m_wifiError;emit wifiChanged();}else m_wifiDebounce.start();});
}
void Connectivity::forgetWifi(const QString &profile){
 if(wifiBusy()||!m_savedProfiles.contains(profile))return;
 call(NM,profile,NM+".Settings.Connection","Delete",{},[this](const QDBusMessage &r){if(r.type()==QDBusMessage::ErrorMessage){m_wifiStatus=errorText(r,"This saved network could not be forgotten.");emit wifiChanged();}else {m_wifiStatus="Saved network forgotten.";refreshSavedWifi();emit wifiChanged();}});
}
void Connectivity::disconnectWifi(const QString &accessPoint){
 const auto ap=m_accessPoints.value(accessPoint);if(wifiBusy()||!ap.value("connected").toBool())return;
 call(NM,ap.value("devicePath").toString(),Device,"Disconnect",{},[this](const QDBusMessage &r){if(r.type()==QDBusMessage::ErrorMessage){m_wifiStatus=errorText(r,"The network could not disconnect.");emit wifiChanged();}else {m_wifiConnecting=false;m_wifiConnectTimeout.stop();m_wifiDebounce.start();}});
}
void Connectivity::connectHiddenWifi(const QString &ssid,const QString &password,const QString &security){
 if(wifiBusy()||m_wifiDevices.isEmpty()||!m_wifiEnabled)return;
 if(ssid.isEmpty()||ssid.toUtf8().size()>32||ssid.contains(QChar(0))||!QStringList{"open","wpa-psk","sae"}.contains(security)){m_wifiStatus="Enter a network name of 1–32 bytes and a supported security type.";emit wifiChanged();return;}
 if(security!="open"&&(password.toUtf8().size()<8||password.toUtf8().size()>63)){m_wifiStatus="Enter a password of 8–63 bytes.";emit wifiChanged();return;}
 RadioInterfaces connection;QVariantMap identity{{"id",ssid},{"uuid",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"type","802-11-wireless"},{"autoconnect",true}};
 if(geteuid()!=0)identity["permissions"]=QStringList{"user:"+qEnvironmentVariable("USER")+":"};connection["connection"]=identity;
 connection["802-11-wireless"]={{"ssid",ssid.toUtf8()},{"mode","infrastructure"},{"hidden",true}};
 if(security!="open"){connection["802-11-wireless"]["security"]="802-11-wireless-security";connection["802-11-wireless-security"]={{"key-mgmt",security},{"psk",password}};}
 connection["ipv4"]={{"method","auto"}};connection["ipv6"]={{"method","auto"}};
 activateWifi(connection,m_wifiDevices.first(),"/",ssid,ssid);
}

void Connectivity::networkDevicesChanged(const QDBusObjectPath &){if(!m_wifiDebounce.isActive())m_wifiDebounce.start();if(m_wifiWatching)refreshSavedWifi();}
void Connectivity::prepareForSleep(bool sleeping){
 if(sleeping){m_wifiTimer.stop();++m_wifiGeneration;++m_scanGeneration;m_wifiBusy=false;m_scanPending=false;m_scanningDevices.clear();m_scanTimeout.stop();m_wifiRoundTimeout.stop();emit wifiChanged();return;}
 m_wifiError.clear();if(m_wifiWatching){m_wifiTimer.start();refreshWifi(true);refreshSavedWifi();}else refreshWifi(false);
}
