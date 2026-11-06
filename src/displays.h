#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QProcess>
#include <QVariantList>

class Displays : public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
 Q_PROPERTY(QVariantMap config READ config NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(bool pending READ pending NOTIFY changed)
 Q_PROPERTY(bool identifying READ identifying NOTIFY changed)
 Q_PROPERTY(QVariantMap identification READ identification NOTIFY changed)
 Q_PROPERTY(QString primary READ primary NOTIFY changed)
public:
 explicit Displays(QObject *parent=nullptr);
 QVariantList outputs() const {return m_outputs;}
 QVariantMap config() const {return m_config;}
 QString status() const {return m_status;}
 bool busy() const {return m_process.state()!=QProcess::NotRunning;}
 bool pending() const {return !m_token.isEmpty();}
 bool identifying() const {return m_identifying;}
 QVariantMap identification() const {return m_identification;}
 QString primary() const;
 Q_INVOKABLE void detect();
 Q_INVOKABLE void identify();
 Q_INVOKABLE void preview(const QVariantMap &proposal);
 Q_INVOKABLE void keep();
 Q_INVOKABLE void revert();
 void restore();
signals:
 void changed();
private:
 void start(const QString &action,const QStringList &arguments={},const QByteArray &input={});
 void loadConfiguration();
 void updateIdentification();
 QProcess m_process;
 QFileSystemWatcher m_watcher;
 QString m_configDirectory,m_runtimeDirectory,m_status,m_action,m_token;
 QVariantList m_outputs;
 QVariantMap m_config;
 QVariantMap m_identification;
 bool m_identifying=false;
};
