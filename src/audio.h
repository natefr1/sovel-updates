#pragma once
#include <QObject>
#include <QVariantList>
#include <QTimer>
#include <QProcess>
#include <QSettings>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <functional>

class Audio : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
 Q_PROPERTY(QVariantList inputs READ inputs NOTIFY changed)
 Q_PROPERTY(QVariantList streams READ streams NOTIFY changed)
 Q_PROPERTY(QVariantList cards READ cards NOTIFY changed)
 Q_PROPERTY(QString defaultOutput READ defaultOutput NOTIFY changed)
 Q_PROPERTY(QString defaultInput READ defaultInput NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(double inputLevel READ inputLevel NOTIFY levelChanged)
 Q_PROPERTY(bool recording READ recording NOTIFY changed)
 Q_PROPERTY(QString lastRecording READ lastRecording NOTIFY changed)
 Q_PROPERTY(int preferredRate READ preferredRate WRITE setPreferredRate NOTIFY changed)
 Q_PROPERTY(int preferredQuantum READ preferredQuantum WRITE setPreferredQuantum NOTIFY changed)
 Q_PROPERTY(bool tuningAvailable READ tuningAvailable NOTIFY changed)
public:
 explicit Audio(QObject *parent=nullptr);
 ~Audio() override;
 QVariantList outputs()const{return m_outputs;}
 QVariantList inputs()const{return m_inputs;}
 QVariantList streams()const{return m_streams;}
 QVariantList cards()const{return m_cards;}
 QString defaultOutput()const{return m_defaultOutput;}
 QString defaultInput()const{return m_defaultInput;}
 QString status()const{return m_status;}
 bool busy()const{return m_busy;}
 double inputLevel()const{return m_level;}
 bool recording()const{return m_record.state()!=QProcess::NotRunning;}
 QString lastRecording()const{return m_lastRecording;}
 int preferredRate()const{return m_settings.value("audio/rate",0).toInt();}
 int preferredQuantum()const{return m_settings.value("audio/quantum",0).toInt();}
 bool tuningAvailable()const{return m_pipewire;}
 Q_INVOKABLE void refresh();
 Q_INVOKABLE void setWatching(bool enabled);
 Q_INVOKABLE void selectOutput(const QString &name);
 Q_INVOKABLE void selectInput(const QString &name);
 Q_INVOKABLE void volume(const QString &name,int percent,bool input=false);
 Q_INVOKABLE void mute(const QString &name,bool muted,bool input=false);
 Q_INVOKABLE void balance(const QString &name,double position);
 Q_INVOKABLE void streamVolume(int id,int percent,bool input=false);
 Q_INVOKABLE void streamMute(int id,bool muted,bool input=false);
 Q_INVOKABLE void moveStream(int id,const QString &name,bool input=false);
 Q_INVOKABLE void profile(const QString &card,const QString &profile);
 Q_INVOKABLE void testSpeaker(const QString &name,int channel);
 Q_INVOKABLE void monitorInput(const QString &name,bool enabled);
 Q_INVOKABLE void recordTest(const QString &name);
 Q_INVOKABLE void stopRecording();
 Q_INVOKABLE void playRecording();
 Q_INVOKABLE void openEqualizer();
 Q_INVOKABLE void restorePreferences();
 void setPreferredRate(int rate);
 void setPreferredQuantum(int quantum);
 static QVariantMap device(const QJsonObject &raw,const QString &selected,bool input);
signals:
 void changed();
 void levelChanged();
private:
 void command(const QStringList &args,std::function<void(bool)> done={});
 void json(const QStringList &args,std::function<void(const QJsonDocument&)> callback);
 void defaults();
 void checkRoutes();
 void requestRoute(const QString &key);
 void forceMetadata(const QString &key,int value);
 bool known(const QString &name,bool input=false)const;
 bool knownStream(int id,bool input)const;
 QSettings m_settings{"Sovel","Desktop"};
 QVariantList m_outputs,m_inputs,m_streams,m_cards;
 QString m_defaultOutput,m_defaultInput,m_status="Checking audio devices…",m_lastRecording;
 QTimer m_poll,m_debounce,m_recordStop,m_routeTimer;
 QMap<QString,QVariantMap> m_routes;
 QProcess m_subscribe,m_meter,m_record;
 bool m_busy=false,m_watching=false,m_refreshAgain=false,m_haveOutputs=false,m_pipewire=false;
 int m_watchers=0;
 double m_level=0;
 int m_pending=0;
};
