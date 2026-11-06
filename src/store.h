#pragma once
#include <QObject>
#include <QProcess>
#include <QVariantList>
#include <QSet>
#include <QNetworkAccessManager>
#include <QVariantMap>
#include <functional>
#include <QTimer>
class Store : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList results READ results NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString permissions READ permissions NOTIFY changed)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QStringList installed READ installed NOTIFY installedChanged)
    Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
    Q_PROPERTY(bool detailsBusy READ detailsBusy NOTIFY detailsChanged)
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)
    Q_PROPERTY(QStringList updatable READ updatable NOTIFY jobsChanged)
public:
    explicit Store(QObject *parent=nullptr);
    static bool allowedOperation(const QStringList &args);
    ~Store() override;
    QVariantList results()const{return m_results;}
    bool busy()const{return m_process!=nullptr;}
    QString status()const{return m_status;}
    QString permissions()const{return m_permissions;}
    QString selectedId()const{return m_selected;}
    QStringList installed()const{return m_installed;}
    QVariantMap details()const{return m_details;}
    bool detailsBusy()const{return m_detailsBusy;}
    QVariantList jobs()const{return m_jobs;}
    QStringList updatable()const{return m_updatable;}
    Q_INVOKABLE void browse(const QString &collection);
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE void refreshInstalled();
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void review(const QString &id);
    Q_INVOKABLE void installSelected();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void open(const QString &id);
    Q_INVOKABLE void updateApp(const QString &id);
    Q_INVOKABLE void cancelJob(const QString &job);
    Q_INVOKABLE void retryJob(const QString &job);
    Q_INVOKABLE void checkAppUpdates();
    Q_INVOKABLE void setWatching(bool enabled){if(enabled){m_jobPoll.start();pollJobs();}else m_jobPoll.stop();}
signals:void changed();void reviewReady();void installedChanged();void detailsChanged();void jobsChanged();
private:
    void pollJobs();
    void startJob(const QString &operation,const QString &id,const QString &scope="user");
    void worker(const QStringList &args,std::function<void(bool,const QVariantMap&)> done);
    QVariantMap normalize(const QVariantMap &raw)const;
    void fetchDetails(const QString &id);
    void ensureSource(std::function<void()> ready);
    void run(const QStringList &args,std::function<void(bool,const QString&)> done);
    bool listed(const QString &id)const;
    QProcess *m_process=nullptr;
    QVariantList m_results;
    QString m_status="Search apps available from Flathub.",m_permissions,m_selected;
    bool m_cancelled=false,m_reviewed=false;
    QStringList m_installed;
    bool m_refreshing=false;
    QNetworkAccessManager m_network;
    QMap<QString,QVariantMap> m_catalog;
    QVariantMap m_details;
    bool m_detailsBusy=false;
    quint64 m_detailGeneration=0;
    QTimer m_jobPoll;
    QVariantList m_jobs;
    QStringList m_updatable;
    QMap<QString,QString> m_updateScopes;
    bool m_pollingJobs=false;
};
