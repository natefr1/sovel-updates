#pragma once
#include <QObject>
#include <QVariantList>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QFutureWatcher>
#include <QProcess>
#include <QSettings>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
class FileBrowser:public QObject {
 Q_OBJECT
 Q_PROPERTY(QString path READ path WRITE navigate NOTIFY pathChanged)
 Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
 Q_PROPERTY(QVariantList places READ places NOTIFY changed)
 Q_PROPERTY(bool loading READ loading NOTIFY changed)
 Q_PROPERTY(bool busy READ busy NOTIFY changed)
 Q_PROPERTY(QString status READ status NOTIFY changed)
 Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY changed)
 Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY changed)
 Q_PROPERTY(bool canBack READ canBack NOTIFY pathChanged)
 Q_PROPERTY(bool canForward READ canForward NOTIFY pathChanged)
 Q_PROPERTY(bool inTrash READ inTrash NOTIFY pathChanged)
 Q_PROPERTY(QString collection READ collection NOTIFY pathChanged)
 Q_PROPERTY(QVariantList volumes READ volumes NOTIFY changed)
 Q_PROPERTY(QString volumeError READ volumeError NOTIFY changed)
 Q_PROPERTY(bool autoMount READ autoMount WRITE setAutoMount NOTIFY changed)
 Q_PROPERTY(bool organizeDownloads READ organizeDownloads WRITE setOrganizeDownloads NOTIFY changed)
 Q_PROPERTY(bool notifyDownloads READ notifyDownloads WRITE setNotifyDownloads NOTIFY changed)
 Q_PROPERTY(QUrl lastDownloadFolder READ lastDownloadFolder NOTIFY changed)
 Q_PROPERTY(QVariantList archiveEntries READ archiveEntries NOTIFY changed)
 Q_PROPERTY(QString operationProgress READ operationProgress NOTIFY changed)
 Q_PROPERTY(QString archiveDestination READ archiveDestination NOTIFY changed)
 Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY changed)
 Q_PROPERTY(QVariantList transfers READ transfers NOTIFY transfersChanged)
 Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
 Q_PROPERTY(int activeTab READ activeTab NOTIFY tabsChanged)
 Q_PROPERTY(bool deepSearch READ deepSearch NOTIFY changed)
public:
 explicit FileBrowser(QObject *parent=nullptr);
 ~FileBrowser()override;
 QString path()const{return m_path;}
 QVariantList entries()const{return m_entries;}
 QVariantList places()const;
 bool loading()const{return m_listing!=nullptr;}
 bool busy()const{return m_process!=nullptr;}
 QString status()const{return m_status;}
 bool showHidden()const{return m_hidden;}
 QString sort()const{return m_sort;}
 bool canBack()const{return m_position>0;}
 bool canForward()const{return m_position+1<m_history.size();}
 bool inTrash()const;
 QString collection()const;
 QVariantList volumes() const {return m_volumes;}
 QString volumeError() const {return m_volumeError;}
 bool autoMount()const{return m_preferences.value("drives/autoMount",false).toBool();}
 void setAutoMount(bool enabled);
 bool organizeDownloads()const{return m_preferences.value("downloads/organize",false).toBool();}
 void setOrganizeDownloads(bool enabled);
 bool notifyDownloads()const{return m_preferences.value("downloads/notify",true).toBool();}
 void setNotifyDownloads(bool enabled);
 QUrl lastDownloadFolder()const;
 QVariantList archiveEntries() const {return m_archiveEntries;}
 QString operationProgress() const {return m_operationProgress;}
 QString archiveDestination() const {return m_archiveDestination;}
 QString viewMode() const;
 void setViewMode(const QString &mode);
 QVariantList transfers() const {return m_transfers;}
 QVariantList tabs() const;
 int activeTab() const {return m_activeTab;}
 bool deepSearch() const {return !m_search.isEmpty();}
 Q_INVOKABLE void navigate(const QString &path);
 Q_INVOKABLE void refresh();
 Q_INVOKABLE void back();
 Q_INVOKABLE void forward();
 Q_INVOKABLE void up();
 void setShowHidden(bool);
 void setSort(const QString &);
 Q_INVOKABLE void createFolder(const QString &name);
 Q_INVOKABLE void rename(const QString &path,const QString &name);
 Q_INVOKABLE void trash(const QStringList &paths);
 Q_INVOKABLE void restore(const QString &path);
 Q_INVOKABLE void copy(const QStringList &paths,bool cut=false);
 Q_INVOKABLE void paste();
 Q_INVOKABLE void open(const QString &path);
 Q_INVOKABLE void refreshVolumes();
 Q_INVOKABLE void volumeAction(const QString &action, const QString &id, const QString &label={});
 Q_INVOKABLE void formatVolume(const QString &id,const QString &filesystem,const QString &confirmation,const QString &identity);
 Q_INVOKABLE void archiveAction(const QString &action, const QString &path,
                               const QString &destination={}, bool newFolder=false, const QString &password={});
 Q_INVOKABLE void compress(const QStringList &paths, const QString &destination, int level);
 Q_INVOKABLE void favorite(const QString &path);
 Q_INVOKABLE void toggleStar(const QString &path);
 Q_INVOKABLE void copyPath(const QString &path);
 Q_INVOKABLE QString downloadsPath() const;
 Q_INVOKABLE void setDownloadsPath(const QUrl &folder);
 Q_INVOKABLE void transferAction(const QString &id,const QString &action);
 Q_INVOKABLE void duplicate(const QString &path);
 Q_INVOKABLE void openResultLocation();
 Q_INVOKABLE void download(const QString &url,const QString &filename,const QUrl &folder={});
 Q_INVOKABLE void newTab();
 Q_INVOKABLE void switchTab(int index);
 Q_INVOKABLE void closeTab(int index);
 Q_INVOKABLE QVariantMap inspect(const QString &path) const;
 static QVariantMap inspectFile(const QString &path);
 Q_INVOKABLE void openLocation(const QUrl &url);
 Q_INVOKABLE void checksum(const QString &path);
 Q_INVOKABLE void searchFolder(const QString &query,const QString &type,int days,int minimumMiB,bool contents);
 Q_INVOKABLE void clearSearch();
 Q_INVOKABLE void transferTo(const QStringList &paths,const QUrl &folder,bool move);
 Q_INVOKABLE void shortcut(const QString &path);
signals:void changed();void pathChanged();void packageRequested(const QString &path);void transfersChanged();void tabsChanged();
private:
 void changePath(const QString &path,bool record);
 bool validSelection(const QStringList &paths)const;
 void run(const QString &program,const QStringList &args,const QString &success,const QString &removeMetadata=QString(),
          const QByteArray &input={});
 void readOperationOutput(QProcess *process);
 void finishOperationOutput(const QString &program, const QStringList &args);
 void refreshTransfers();
 void beginTransfer(const QStringList &paths,const QString &destination,bool move);
 void enqueueTransfer(QJsonObject job);
 void recordRecent(const QString &path);
 QString m_path,m_status,m_sort="name";
 QStringList m_history;int m_position=-1;
 bool m_hidden=false,m_reload=false;
 QVariantList m_entries;
 QFileSystemWatcher m_watcher;QTimer m_debounce;
 QFutureWatcher<QVariantList> *m_listing=nullptr;
 QProcess *m_process=nullptr;
 QProcess *m_volumeProbe=nullptr;
 QTimer m_volumeTimer;
 QVariantList m_volumes,m_archiveEntries;
 QSet<QString> m_autoMountAttempted;
 QString m_volumeError,m_operationProgress,m_archiveDestination;
 QByteArray m_operationOutput;
 QJsonObject m_operationResult;
 QSettings m_preferences{"Sovel","Files"};
 QVariantList m_transfers;
 QTimer m_transferTimer;
 struct Tab {QStringList history;int position;};
 QList<Tab> m_tabs{{{},-1}};
 int m_activeTab=0;
 QVariantMap m_search;
};
