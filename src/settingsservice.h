#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QSettings>
#include <QTimer>
#include <QPointer>
#include <QProcess>
#include <QUrl>
#include <QSet>
#include <functional>
class SettingsService:public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList pages READ pages CONSTANT)
 Q_PROPERTY(QVariantMap definitions READ definitions CONSTANT)
 Q_PROPERTY(QVariantMap preferences READ preferences NOTIFY changed)
 Q_PROPERTY(QVariantMap colors READ colors NOTIFY changed)
 Q_PROPERTY(QVariantMap reports READ reports NOTIFY reportsChanged)
 Q_PROPERTY(QString status READ status NOTIFY statusChanged)
 Q_PROPERTY(bool busy READ busy NOTIFY statusChanged)
 Q_PROPERTY(QUrl avatarUrl READ avatarUrl NOTIFY changed)
public:
 explicit SettingsService(QObject *parent=nullptr,bool applyUserChanges=true);
 ~SettingsService()override;
 QVariantList pages()const{return m_pages;}
 QVariantMap definitions()const{return m_definitions;}
 QVariantMap preferences()const{return m_preferences;}
 QVariantMap colors()const;
 QVariantMap reports()const{return m_reports;}
 QString status()const{return m_status;}
 bool busy()const{return !m_jobs.isEmpty();}
 QUrl avatarUrl()const;
 Q_INVOKABLE bool setAvatar(const QUrl &file);
 Q_INVOKABLE bool setPreference(const QString &key,const QVariant &value);
 Q_INVOKABLE void refresh(const QString &page,bool force=false);
 Q_INVOKABLE void perform(const QString &operation,const QVariantMap &payload=QVariantMap());
 Q_INVOKABLE void visit(const QString &page);
 Q_INVOKABLE void favorite(const QString &page,bool enabled);
 Q_INVOKABLE QVariantMap readProfile(const QUrl &file);
 Q_INVOKABLE bool applyProfile(const QVariantMap &profile);
 Q_INVOKABLE bool exportProfile(const QUrl &file);
 Q_INVOKABLE bool exportDiagnostics(const QUrl &file);
 Q_INVOKABLE void resetAppearance();
signals:
 void changed();
 void reportsChanged();
 void statusChanged();
 void actionFinished(const QString &operation,bool success,const QVariantMap &result);
private:
 void reload();
 bool save();
 bool valid(const QString &key,const QVariant &value)const;
 bool pageExists(const QString &id)const;
 QString helper(bool admin=false)const;
 void run(const QString &operation,const QVariantMap &payload,bool admin,std::function<void(bool,const QVariantMap&)> callback);
 void applyPreferences();
 void updateApplicationTheme();
 QString m_file,m_status;
 QVariantList m_pages;
 QVariantMap m_definitions,m_preferences,m_reports,m_extra;
 QSettings m_aliases{"Sovel","Desktop"};
 QTimer m_poll;
 QMap<QString,QPointer<QProcess>> m_jobs;
 QMap<QString,qint64> m_refreshed;
 bool m_applyUserChanges=true;
 bool m_applyPending=false;
 int m_themeHour=-1;
};
