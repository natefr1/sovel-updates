#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QSet>
#include <QSettings>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <functional>

using RadioInterfaces=QMap<QString,QVariantMap>;
using RadioObjects=QMap<QDBusObjectPath,RadioInterfaces>;
Q_DECLARE_METATYPE(RadioInterfaces)
Q_DECLARE_METATYPE(RadioObjects)

class Connectivity : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool wifiAvailable READ wifiAvailable NOTIFY wifiChanged)
    Q_PROPERTY(bool wifiEnabled READ wifiEnabled NOTIFY wifiChanged)
    Q_PROPERTY(bool wifiBusy READ wifiBusy NOTIFY wifiChanged)
    Q_PROPERTY(QString wifiStatus READ wifiStatus NOTIFY wifiChanged)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY wifiChanged)
    Q_PROPERTY(QVariantList savedWifi READ savedWifi NOTIFY wifiChanged)
    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable NOTIFY bluetoothChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled NOTIFY bluetoothChanged)
    Q_PROPERTY(bool bluetoothBusy READ bluetoothBusy NOTIFY bluetoothChanged)
    Q_PROPERTY(QString bluetoothStatus READ bluetoothStatus NOTIFY bluetoothChanged)
    Q_PROPERTY(QVariantList bluetoothDevices READ bluetoothDevices NOTIFY bluetoothChanged)
public:
    explicit Connectivity(QObject *parent=nullptr);
    Connectivity(const QDBusConnection &bus,QObject *parent);
    ~Connectivity() override;
    bool wifiAvailable() const{return m_wifiAvailable;}
    bool wifiEnabled() const{return m_wifiEnabled;}
    bool wifiBusy() const{return m_wifiBusy||m_wifiConnecting||m_scanPending;}
    QString wifiStatus() const{return m_wifiStatus;}
    QVariantList wifiNetworks() const{return m_networks;}
    QVariantList savedWifi() const{return m_savedNetworks;}
    bool bluetoothAvailable() const{return !m_adapters.isEmpty();}
    bool bluetoothEnabled() const{return m_bluetoothEnabled;}
    bool bluetoothBusy() const{return m_bluetoothBusy;}
    QString bluetoothStatus() const{return m_bluetoothStatus;}
    QVariantList bluetoothDevices() const{return m_devices;}
    Q_INVOKABLE void startWifi();
    Q_INVOKABLE void stopWifi();
    Q_INVOKABLE void scanWifi();
    Q_INVOKABLE void inspectWifi(){refreshWifi(false);}
    Q_INVOKABLE void connectSavedWifi(const QString &profile);
    Q_INVOKABLE void forgetWifi(const QString &profile);
    Q_INVOKABLE void disconnectWifi(const QString &accessPoint);
    Q_INVOKABLE void connectHiddenWifi(const QString &ssid,const QString &password,const QString &security);
    Q_INVOKABLE void setWifiEnabled(bool enabled);
    Q_INVOKABLE void connectWifi(const QString &accessPoint,const QString &password);
    Q_INVOKABLE void startBluetooth();
    Q_INVOKABLE void inspectBluetooth(){refreshBluetooth(false);}
    Q_INVOKABLE void stopBluetooth();
    Q_INVOKABLE void scanBluetooth();
    Q_INVOKABLE void setBluetoothEnabled(bool enabled);
    Q_INVOKABLE void connectBluetooth(const QString &device);
    Q_INVOKABLE void disconnectBluetooth(const QString &device);
    Q_INVOKABLE void forgetBluetooth(const QString &device);
    Q_INVOKABLE void renameBluetooth(const QString &device,const QString &name);
signals:
    void wifiChanged();
    void bluetoothChanged();
private slots:
    void networkDevicesChanged(const QDBusObjectPath &);
    void prepareForSleep(bool sleeping);
    void propertiesChanged(const QString &interface,const QVariantMap &changes,const QStringList &invalidated,const QDBusMessage &message);
    void bluezAdded(const QDBusObjectPath &,const RadioInterfaces &);
    void bluezRemoved(const QDBusObjectPath &,const QStringList &);
    void deviceStateChanged(uint state,uint oldState,uint reason,const QDBusMessage &message);
private:
    using Reply=std::function<void(const QDBusMessage&)>;
    void call(const QString &service,const QString &path,const QString &interface,const QString &method,const QVariantList &arguments,Reply reply);
    void refreshWifi(bool scan);
    void refreshSavedWifi();
    void activateWifi(const RadioInterfaces &connection,const QString &device,const QString &specific,const QString &label,const QString &ssid);
    void refreshBluetooth(bool discover=false);
    void bluetoothConnect(const QString &device);
    void setBluezProperty(const QString &path,const QString &interface,const QString &name,const QVariant &value,Reply reply);
    QDBusConnection m_bus;
    QSettings m_names{"Sovel","Desktop"};
    QTimer m_wifiTimer,m_wifiDebounce,m_wifiConnectTimeout,m_bluetoothDebounce,m_scanTimeout,m_wifiRoundTimeout;
    bool m_scanPending=false,m_wifiRefreshAgain=false;
    bool m_wifiWatching=false,m_bluetoothWatching=false;
    bool m_wifiAvailable=false,m_wifiEnabled=false,m_wifiBusy=false;
    bool m_wifiConnecting=false;
    bool m_bluetoothEnabled=false,m_bluetoothBusy=false;
    quint64 m_wifiGeneration=0;
    quint64 m_scanGeneration=0;
    QString m_wifiStatus="Checking Wi-Fi…",m_bluetoothStatus="Checking Bluetooth…";
    QVariantList m_networks,m_devices,m_savedNetworks;
    QMap<QString,QVariantMap> m_savedProfiles;
    QStringList m_wifiDevices;
    QSet<QString> m_scanningDevices;
    bool m_savedBusy=false;
    quint64 m_savedGeneration=0;
    QMap<QString,QVariantMap> m_accessPoints,m_bluetoothPaths;
    QStringList m_adapters,m_discoveryLeases;
    QSet<QString> m_startingDiscovery;
    QString m_pendingWifi,m_pendingWifiDevice,m_pendingWifiSsid,m_wifiError;
};
