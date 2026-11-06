#pragma once
#include <QObject>
#include <QVariantList>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QPointer>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY metricsChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY preferencesChanged)
    Q_PROPERTY(bool lowEffects READ lowEffects WRITE setLowEffects NOTIFY preferencesChanged)
    Q_PROPERTY(QString wallpaper READ wallpaper WRITE setWallpaper NOTIFY preferencesChanged)
    Q_PROPERTY(QUrl wallpaperUrl READ wallpaperUrl NOTIFY preferencesChanged)
    Q_PROPERTY(bool externalWallpaper READ externalWallpaper NOTIFY preferencesChanged)
    Q_PROPERTY(QVariantList pinnedApps READ pinnedApps NOTIFY preferencesChanged)
    Q_PROPERTY(QVariantMap systemInfo READ systemInfo CONSTANT)
    Q_PROPERTY(QString username READ username CONSTANT)
    Q_PROPERTY(QString home READ home CONSTANT)
    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(QString updateReport READ updateReport NOTIFY updateReportChanged)
    Q_PROPERTY(QVariantMap audio READ audio NOTIFY audioChanged)
    Q_PROPERTY(QVariantMap shellPreferences READ shellPreferences NOTIFY preferencesChanged)
    Q_PROPERTY(QString networkReport READ networkReport NOTIFY networkChanged)
    Q_PROPERTY(QVariantMap power READ power NOTIFY powerChanged)
    Q_PROPERTY(QString mode READ mode CONSTANT)
    Q_PROPERTY(bool privateMode READ privateMode CONSTANT)
    Q_PROPERTY(bool privateConfigured READ privateConfigured CONSTANT)
    Q_PROPERTY(QVariantMap vault READ vault NOTIFY vaultChanged)
    Q_PROPERTY(bool vaultBusy READ vaultBusy NOTIFY vaultChanged)
public:
    explicit Backend(QObject *parent=nullptr);
    QVariantMap metrics() const { return m_metrics; }
    QString username() const;
    QString home() const;
    bool reducedMotion() const;
    bool lowEffects() const;
    void setReducedMotion(bool);
    void setLowEffects(bool);
    QString wallpaper() const;
    QUrl wallpaperUrl() const;
    Q_INVOKABLE bool setWallpaperFile(const QUrl &file);
    Q_INVOKABLE void claimMenu(QObject *menu);
    Q_INVOKABLE void releaseShellInput();
    void setWallpaper(const QString &);
    bool externalWallpaper()const;
    void restoreExternalWallpaper();
    void openWallpaperPicker();
    QVariantMap systemInfo() const;
    QString notice() const { return m_notice; }
    QString updateReport() const { return m_updateReport; }
    Q_INVOKABLE void launch(const QString &app);
    Q_INVOKABLE void launchJared(const QString &prompt);
    Q_INVOKABLE QVariantList applications(bool refresh=false) const;
    QVariantList pinnedApps() const;
    Q_INVOKABLE void setPinned(const QString &id, bool pinned);
    Q_INVOKABLE bool isPinned(const QString &id) const;
    Q_INVOKABLE void setDockOrder(const QStringList &ids);
    Q_INVOKABLE void movePinned(const QString &id,int direction);
    Q_INVOKABLE void launchApplication(const QString &id);
    Q_INVOKABLE QVariantList files(const QString &path) const;
    Q_INVOKABLE bool openFile(const QString &path);
    Q_INVOKABLE bool makeFolder(const QString &parent, const QString &name);
    Q_INVOKABLE QVariantList widgets() const;
    Q_INVOKABLE void saveWidgets(const QVariantList &widgets);
    Q_INVOKABLE void checkUpdates();
    QVariantMap audio() const { return m_audio; }
    Q_INVOKABLE void refreshAudio();
    QString networkReport() const { return m_networkReport; }
    Q_INVOKABLE void refreshNetwork();
    QVariantMap power() const { return m_power; }
    Q_INVOKABLE void refreshPower();
    Q_INVOKABLE void setPowerProfile(const QString &profile);
    Q_INVOKABLE void setVolume(int percent);
    Q_INVOKABLE void setMuted(bool muted);
    QVariantMap shellPreferences()const;
    Q_INVOKABLE void setShellPreference(const QString &name,const QVariant &value);
    Q_INVOKABLE void message(const QString &text);
    QString mode()const;
    bool privateMode()const;
    bool privateConfigured()const;
    // The greeter leaves the private VT through logind; polkit allows this for the active seat session.
    Q_INVOKABLE bool switchToNormalDesktop();
    QVariantMap vault()const{return m_vault;}
    bool vaultBusy()const{return m_vaultBusy;}
    Q_INVOKABLE void refreshVault();
    Q_INVOKABLE void vaultAction(const QString &action,QString passphrase=QString());
signals:
    void metricsChanged();
    void preferencesChanged();
    void noticeChanged();
    void updateReportChanged();
    void audioChanged();
    void networkChanged();
    void powerChanged();
    void vaultChanged();
private:
    QVariantMap m_vault;
    bool m_vaultBusy=false;
    QStringList dockIds()const;
    void sample();
    QSettings m_settings;
    QPointer<QObject> m_openMenu;
    QVariantMap m_metrics;
    QString m_notice, m_updateReport="Not checked";
    QTimer m_timer;
    quint64 m_total=0, m_idle=0;
    bool m_checking=false;
    int m_previousMotion=-1,m_previousEffects=-1;
    QString m_previousWallpaper,m_previousPhoto;
    bool m_previousExternalWallpaper=false;
    QStringList m_previousPins;
    QVariantMap m_previousShell;
    mutable QVariantList m_applicationCache;
    mutable qint64 m_applicationCacheTime=0;
    QVariantMap m_audio;
    bool m_audioBusy=false;
    bool m_networkBusy=false;
    bool m_powerBusy=false;
    QVariantMap m_power;
    QString m_networkReport="Checking connections…";
    void audioCommand(const QStringList &args);
};
