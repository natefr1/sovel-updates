#pragma once
#include <QObject>
#include <QPointer>
#include <QSettings>
#include <QVariantList>
#include <QTimer>
#include <PackageKit/Transaction>

class Updates : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool canInstall READ canInstall NOTIFY changed)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString lastChecked READ lastChecked NOTIFY changed)
    Q_PROPERTY(QVariantList packages READ packages NOTIFY changed)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY changed)
    Q_PROPERTY(bool liveSession READ liveSession CONSTANT)
    Q_PROPERTY(bool checked READ checked NOTIFY changed)
    Q_PROPERTY(bool installing READ installing NOTIFY changed)
    Q_PROPERTY(bool automaticChecks READ automaticChecks WRITE setAutomaticChecks NOTIFY changed)
    Q_PROPERTY(QVariantMap releaseDetails READ releaseDetails NOTIFY changed)
    Q_PROPERTY(QVariantList releaseHistory READ releaseHistory NOTIFY changed)
public:
    explicit Updates(QObject *parent=nullptr);
    bool busy() const{return !m_transaction.isNull();}
    bool canInstall() const{return !liveSession()&&!busy()&&m_checked&&!m_ids.isEmpty();}
    bool canCancel() const{return m_transaction&&m_transaction->allowCancel();}
    int progress() const{return m_transaction&&m_transaction->percentage()<=100?int(m_transaction->percentage()):-1;}
    QString status() const{return m_status;}
    QString error() const{return m_error;}
    QString lastChecked() const{return m_settings.value("updates/lastChecked").toString();}
    QVariantList packages() const{return m_packages;}
    bool restartRequired() const;
    bool liveSession() const;
    bool checked() const{return m_checked;}
    bool installing() const{return busy()&&m_operation==Operation::Install;}
    bool automaticChecks() const{return m_settings.value("updates/automaticChecks",true).toBool();}
    QVariantMap releaseDetails()const{return m_releaseDetails;}
    QVariantList releaseHistory()const{return m_releaseHistory;}
    Q_INVOKABLE void refreshReleaseInfo();
    void setAutomaticChecks(bool enabled);
    void startAutomaticChecks();
    void pollAutomaticChecks();
    Q_INVOKABLE void check();
    Q_INVOKABLE void install();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void restart();
signals:
    void changed();
private:
    enum class Operation {Refresh,List,Install};
    void attach(PackageKit::Transaction *transaction,Operation operation);
    void list();
    QPointer<PackageKit::Transaction> m_transaction;
    QSettings m_settings;
    QTimer m_automaticTimer;
    Operation m_operation=Operation::Refresh;
    bool m_background=false;
    QString m_status="Check for the latest Sovel and system updates.",m_error;
    QVariantList m_packages;
    QStringList m_ids;
    bool m_checked=false,m_restart=false;
    QVariantMap m_releaseDetails;
    QVariantList m_releaseHistory;
    bool m_releaseInfoBusy=false;
};
