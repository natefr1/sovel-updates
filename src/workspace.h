#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QFileSystemWatcher>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QProcess>
#include <QSet>
class Backend;
class SettingsService;
class Workspace:public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList registry READ registry CONSTANT)
 Q_PROPERTY(QVariantList layout READ layout NOTIFY layoutChanged)
 Q_PROPERTY(QVariantMap data READ data NOTIFY dataChanged)
 Q_PROPERTY(QVariantMap telemetry READ telemetry NOTIFY telemetryChanged)
 Q_PROPERTY(QVariantMap weather READ weather NOTIFY weatherChanged)
 Q_PROPERTY(QVariantList cities READ cities NOTIFY weatherChanged)
 Q_PROPERTY(QVariantMap media READ media NOTIFY mediaChanged)
 Q_PROPERTY(QVariantList usage READ usage NOTIFY usageChanged)
 Q_PROPERTY(QString status READ status NOTIFY statusChanged)
 Q_PROPERTY(QVariantMap assistant READ assistant NOTIFY assistantChanged)
public:
 Workspace(Backend *backend,SettingsService *settings,bool desktop,QObject *parent=nullptr);
 ~Workspace()override;
 QVariantList registry()const{return m_registry;}
 QVariantList layout()const{return m_layout;}
 QVariantMap data()const{return m_data;}
 QVariantMap telemetry()const{return m_telemetry;}
 QVariantMap weather()const{return m_weather;}
 QVariantList cities()const{return m_cities;}
 QVariantMap media()const{return m_media;}
 QVariantList usage()const;
 QString status()const{return m_status;}
 QVariantMap assistant()const{return m_assistant;}
 static QString assistantStatePath();
 Q_INVOKABLE void assistantCommand(const QString &action,const QString &text={});
 Q_INVOKABLE QString addWidget(const QString &type);
 Q_INVOKABLE void removeWidget(const QString &id);
 Q_INVOKABLE bool moveWidget(const QString &id,double x,double y);
 Q_INVOKABLE bool placeWidget(const QString &id,const QString &output,double x,double y,int width,int height);
 Q_INVOKABLE bool undoWidgetPlacement(const QString &output);
 Q_INVOKABLE bool saveLayout(const QVariantList &layout);
 Q_INVOKABLE bool configureWidget(const QString &id,const QString &size,int opacity,int refresh);
 Q_INVOKABLE bool setWidgetData(const QString &id,const QVariantMap &data);
 Q_INVOKABLE void setDesktopActive(bool active){m_active=active;}
 Q_INVOKABLE void setForeground(const QString &appId);
 Q_INVOKABLE void clearUsage();
 Q_INVOKABLE void searchCity(const QString &query);
 Q_INVOKABLE void chooseCity(int index);
 Q_INVOKABLE void refreshWeather();
 Q_INVOKABLE void mediaAction(const QString &action);
 Q_INVOKABLE QString calculate(const QString &expression)const;
 Q_INVOKABLE QString timeInZone(const QString &zone)const;
 Q_INVOKABLE void notify(const QString &title,const QString &message);
signals:
 void layoutChanged();void dataChanged();void telemetryChanged();void weatherChanged();void mediaChanged();void usageChanged();void statusChanged();void assistantChanged();
private:
 void readAssistantState();
 void reload();bool save();void tick();void sampleTelemetry();void sampleGpu();void sampleMedia();
 bool knownType(const QString &type)const;
 bool hasWidget(const QString &type)const;
 bool networkAllowed()const;
 Backend *m_backend;SettingsService *m_settings;
 bool m_desktop=false,m_active=false,m_gpuBusy=false,m_mediaBusy=false,m_weatherBusy=false;
 QString m_file,m_status,m_foreground,m_mediaService;
 QVariantList m_registry,m_layout,m_cities;
 QVariantMap m_data,m_telemetry,m_weather,m_media,m_usage,m_extra;
 QTimer m_poll;
 QFileSystemWatcher m_assistantWatcher;
 QVariantMap m_assistant;
 QElapsedTimer m_tickClock;
 qint64 m_lastTick=0,m_lastSave=0,m_lastGpu=0,m_lastMedia=0,m_lastWeather=0;
 quint64 m_rx=0,m_tx=0;
 qint64 m_networkSample=0;
 qint64 m_lastDownloads=0;
 QStringList m_downloads;
 QNetworkAccessManager m_network;
 QPointer<QProcess> m_gpu;
 QSet<QString> m_dirty;
 struct Placement {QString id;QVariantMap previous;};
 QHash<QString,QList<Placement>> m_placementHistory;
 double m_breakElapsed=0;
 quint64 m_weatherGeneration=0;
};
