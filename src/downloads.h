#pragma once
// The Downloads hub behind the menu bar button and Settings → Apps & Defaults
// → Downloads. Browsers own their downloads, so Sovel watches the Downloads
// folder: files still being written show as in progress, and a notification
// arrives when one completes. Each account watches only its own folder.
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantList>

class SettingsService;

class Downloads : public QObject {
 Q_OBJECT
 Q_PROPERTY(QString folder READ folder NOTIFY changed)
 Q_PROPERTY(QVariantList items READ items NOTIFY changed)
 Q_PROPERTY(int activeCount READ activeCount NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
public:
 explicit Downloads(SettingsService *settings,bool notify,QObject *parent=nullptr);
 QString folder()const{return m_folder;}
 QVariantList items()const{return m_items;}
 int activeCount()const{return m_active;}
 QString status()const{return m_status;}
 Q_INVOKABLE void open(const QString &path);
 Q_INVOKABLE void showInFolder(const QString &path);
 Q_INVOKABLE void openFolder();
 Q_INVOKABLE bool setFolder(const QString &path);
 Q_INVOKABLE void refresh();
 static QString partialTarget(const QString &name);
 static QString configuredFolder();
signals:
 void changed();
 void completed(const QString &name,const QString &path);
private:
 void scan(bool announce);
 bool inside(const QString &path)const;
 SettingsService *m_settings;bool m_notify;
 QString m_folder,m_status;QVariantList m_items;int m_active=0;bool m_scanned=false;
 QHash<QString,qint64> m_known;     // complete file name -> size
 QHash<QString,QString> m_partials; // partial file name -> final name
 QFileSystemWatcher m_watcher;QTimer m_debounce,m_progress;
};
