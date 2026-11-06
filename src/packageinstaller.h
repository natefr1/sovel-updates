#pragma once
#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QPointer>
#include <PackageKit/Transaction>
#include <memory>
class PackageInstaller:public QObject {
 Q_OBJECT
 Q_PROPERTY(QVariantMap details READ details NOTIFY changed)
 Q_PROPERTY(QVariantList changes READ changes NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(QString error READ error NOTIFY changed)
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(bool reviewed READ reviewed NOTIFY changed)
 Q_PROPERTY(bool installed READ installed NOTIFY changed)
 Q_PROPERTY(int progress READ progress NOTIFY changed)
 Q_PROPERTY(bool canCancel READ canCancel NOTIFY changed)
public:
 explicit PackageInstaller(QObject *parent=nullptr):QObject(parent){}
 ~PackageInstaller()override;
 QVariantMap details()const{return m_details;}
 QVariantList changes()const{return m_changes;}
 QString status()const{return m_status;}
 QString error()const{return m_error;}
 bool busy()const{return m_loading||m_transaction;}
 bool reviewed()const{return m_reviewed;}
 bool installed()const{return m_installed;}
 int progress()const{return m_transaction&&m_transaction->percentage()<=100?int(m_transaction->percentage()):-1;}
 bool canCancel()const{return m_transaction&&m_transaction->allowCancel();}
 Q_INVOKABLE void load(const QString &path);
 Q_INVOKABLE void review();
 Q_INVOKABLE void install();
 Q_INVOKABLE void cancel();
signals:void changed();
private:
 void attach(PackageKit::Transaction *transaction,bool simulate);
 QVariantMap m_details;QVariantList m_changes;
 QString m_file,m_hash,m_status="Choose a downloaded Debian package.",m_error;
 bool m_loading=false,m_reviewed=false,m_installed=false;
 std::shared_ptr<QTemporaryDir> m_stage;
 QPointer<PackageKit::Transaction> m_transaction;
};
