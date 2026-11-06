#pragma once
#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QSaveFile>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <QFutureWatcher>
#include <memory>
#include <QPointer>
#include <atomic>
#include <functional>
class Backend;class Audio;class Connectivity;

class Jared : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(bool modelReady READ modelReady NOTIFY changed)
 Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
 Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY changed)
 Q_PROPERTY(QVariantMap pendingAction READ pendingAction NOTIFY changed)
 Q_PROPERTY(bool allowBasicActions READ allowBasicActions WRITE setAllowBasicActions NOTIFY changed)
 Q_PROPERTY(bool keepBackground READ keepBackground WRITE setKeepBackground NOTIFY changed)
 Q_PROPERTY(bool speakReplies READ speakReplies WRITE setSpeakReplies NOTIFY changed)
 Q_PROPERTY(int idleSeconds READ idleSeconds WRITE setIdleSeconds NOTIFY changed)
 Q_PROPERTY(QString executionMode READ executionMode WRITE setExecutionMode NOTIFY changed)
 Q_PROPERTY(QVariantMap workspaces READ workspaces NOTIFY changed)
 Q_PROPERTY(QVariantMap workRecord READ workRecord NOTIFY changed)
 Q_PROPERTY(QVariantList fileMatches READ fileMatches NOTIFY changed)
 Q_PROPERTY(QString articleText READ articleText NOTIFY changed)
 Q_PROPERTY(bool recording READ recording NOTIFY changed)
 Q_PROPERTY(QString transcript READ transcript NOTIFY changed)
 Q_PROPERTY(QString whisperModel READ whisperModel NOTIFY changed)
 Q_PROPERTY(QString piperVoice READ piperVoice NOTIFY changed)
 Q_PROPERTY(QString modelPreference READ modelPreference WRITE setModelPreference NOTIFY changed)
 Q_PROPERTY(QString runtimeLabel READ runtimeLabel NOTIFY changed)
 Q_PROPERTY(QVariantMap sessionState READ sessionState NOTIFY changed)
 Q_PROPERTY(QStringList availableDisplays READ availableDisplays NOTIFY changed)
public:
 Jared(Backend *backend,Audio *audio,Connectivity *radio,QObject *parent=nullptr);
 ~Jared()override;
 QVariantList messages()const{return m_messages;}
 QString status()const{return m_status;}
 bool busy()const{return m_busy;}
 bool modelReady()const{return m_modelReady;}
 bool loaded()const{return m_loaded;}
 int downloadProgress()const{return m_downloadProgress;}
 QVariantMap pendingAction()const{return m_pending;}
 bool allowBasicActions()const{return m_settings.value("jared/allowBasic",false).toBool();}
 bool keepBackground()const{return m_settings.value("jared/background",false).toBool();}
 bool speakReplies()const{return m_settings.value("jared/speak",false).toBool();}
 int idleSeconds()const{return m_settings.value("jared/idle",120).toInt();}
 QString executionMode()const{return m_settings.value("jared/execution","cpu").toString();}
 QVariantMap workspaces() const {return m_workspaces;}
 QVariantMap workRecord() const {return m_workRecord;}
 QVariantList fileMatches() const {return m_fileMatches;}
 QString articleText() const {return m_articleText;}
 bool recording() const {return m_recording;}
 QString transcript() const {return m_transcript;}
 QString whisperModel() const {return m_settings.value("jared/whisperModel").toString();}
 QString piperVoice() const {return m_settings.value("jared/piperVoice").toString();}
 QString modelPreference() const {return m_settings.value("jared/modelPreference","auto").toString();}
 QString runtimeLabel() const {return m_runtimeLabel;}
 QVariantMap sessionState() const;
 QStringList availableDisplays() const;
 void setModelPreference(const QString &preference);
 void setAppearanceHandler(std::function<bool(const QString &)> handler) {m_appearanceHandler=std::move(handler);}
 void setAllowBasicActions(bool);
 void setKeepBackground(bool);
 void setSpeakReplies(bool);
 void setIdleSeconds(int);
 void setExecutionMode(const QString&);
 Q_INVOKABLE void ask(const QString &text);
 Q_INVOKABLE void approveAction();
 Q_INVOKABLE void chooseOutput(int index);
 Q_INVOKABLE void rejectAction();
 Q_INVOKABLE void downloadModel();
 Q_INVOKABLE void verifyModel();
 Q_INVOKABLE void cancel();
 Q_INVOKABLE void clearHistory();
 Q_INVOKABLE void unload();
 Q_INVOKABLE void sayLastReply();
 Q_INVOKABLE void work(const QString &operation,const QVariantMap &arguments);
 Q_INVOKABLE void summarize(const QString &text,const QString &source);
 Q_INVOKABLE void speak(const QString &text);
 Q_INVOKABLE void setVoiceFile(const QString &kind,const QUrl &file);
 Q_INVOKABLE void recordVoice();
 Q_INVOKABLE void stopVoice();
 Q_INVOKABLE void clearTranscript();
 Q_INVOKABLE void downloadGpuModel();
 static QVariantMap classify(const QString &text);
signals:
 void changed();
private:
 friend class JaredTests;
 void add(const QString &role,const QString &text);
 void answer(const QString &text);
 void propose(QVariantMap action);
 void beginConversation(const QString &text);
 void startServer();
 void health();
 void completeChat();
 void interpretReply(const QString &reply);
 QString systemSummary(const QString &kind)const;
 void finishAction(bool ok,const QString &text);
 QString modelPath()const;
 bool cachedVerification()const;
 void markVerified();
 void saveHistory();
 bool workIntent(const QString &text);
 void workFinished(const QString &operation,const QJsonObject &result);
 void chooseRuntime();
 void startLightweightModel();
 void verifyGpuRuntime();
 void initializeVoice();
 void actionFeedback(bool ok);
 Backend *m_backend;Audio *m_audio;Connectivity *m_radio;
 QSettings m_settings{"Sovel","Desktop"};
 QNetworkAccessManager m_network,m_downloadNetwork;
 QProcess m_server,m_speech;
 QProcess m_recorder,m_transcriber,m_voice,m_gpuDownload;
 QTimer m_recordLimit,m_transcribeLimit;
 QPointer<QProcess> m_workProcess;
 QTimer m_health,m_startTimeout,m_idle,m_actionTimeout,m_feedbackTimer;
 QPointer<QNetworkReply> m_chat,m_download;
 QFutureWatcher<QByteArray> m_verification;
 std::shared_ptr<std::atomic_bool> m_verifyCancelled;
 std::unique_ptr<QSaveFile> m_file;
 QCryptographicHash m_hash{QCryptographicHash::Sha256};
 QTemporaryDir m_runtime;
 QVariantList m_messages;
 QVariantMap m_pending,m_model;
 QVariantMap m_workspaces,m_workRecord;
 QVariantMap m_projectRecent;
 QVariantList m_fileMatches;
 QString m_articleText,m_readOnlyContext;
 bool m_summaryOnly=false;
 QString m_transcript,m_runtimeLabel="Local processing",m_ollamaHost;
 QString m_feedback;
 bool m_recording=false,m_recordAccepted=false,m_useOllama=false;
 std::function<bool(const QString &)> m_appearanceHandler;
 QString m_status="Local Qwen3-1.7B · no conversation is sent to a cloud service.",m_prompt,m_token,m_action,m_expected,m_queued,m_lastTopic;
 bool m_busy=false,m_modelReady=false,m_loaded=false,m_changeRequested=false,m_stopping=false;
 int m_port=0,m_downloadProgress=-1;
 quint64 m_runtimeGeneration=0,m_requestGeneration=0;
};
