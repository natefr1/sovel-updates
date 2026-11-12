#pragma once
// Sovel's file picker, served as the org.freedesktop.impl.portal.FileChooser
// backend. Apps that use the XDG portal (browsers, GTK and flatpak apps) get
// this dialog instead of the GTK one. Each account runs its own copy, so
// Normal Mode never sees private folders.
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QObject>
#include <QPointer>
#include <QQuickView>
#include <QTimer>
#include <QVariantMap>

class Backend;
class SettingsService;

// One open dialog. QML reads the request and calls accept() or cancel().
class FilePickerSession : public QObject {
 Q_OBJECT
 Q_PROPERTY(QString title MEMBER m_title CONSTANT)
 Q_PROPERTY(QString kind MEMBER m_kind CONSTANT)            // open, save, folder, save-files
 Q_PROPERTY(bool multiple MEMBER m_multiple CONSTANT)
 Q_PROPERTY(QString acceptLabel MEMBER m_acceptLabel CONSTANT)
 Q_PROPERTY(QVariantList filters MEMBER m_filters CONSTANT)
 Q_PROPERTY(int currentFilter MEMBER m_currentFilter CONSTANT)
 Q_PROPERTY(QString startFolder MEMBER m_startFolder CONSTANT)
 Q_PROPERTY(QString currentName MEMBER m_currentName CONSTANT)
 Q_PROPERTY(QStringList saveNames MEMBER m_saveNames CONSTANT)
 Q_PROPERTY(QVariantList places READ places CONSTANT)
 Q_PROPERTY(QVariantList drives READ drives NOTIFY drivesChanged)
 Q_PROPERTY(QVariantList recent READ recent CONSTANT)
public:
 struct Request {QString title,kind,acceptLabel,startFolder,currentName,currentFilterName;bool multiple=false;QVariantList filters;QStringList saveNames;};
 FilePickerSession(const Request &request,QObject *parent=nullptr);
 QVariantList places()const;
 QVariantList drives()const;
 QVariantList recent()const;
 Q_INVOKABLE bool exists(const QString &path)const;
 Q_INVOKABLE bool isFolder(const QString &path)const;
 Q_INVOKABLE QString parentOf(const QString &path)const;
 Q_INVOKABLE QString join(const QString &folder,const QString &name)const;
 Q_INVOKABLE QString expand(const QString &typed,const QString &folder)const;
 Q_INVOKABLE QString makeFolder(const QString &folder,const QString &name);
 Q_INVOKABLE QString displayPath(const QString &path)const;
 Q_INVOKABLE void refreshDrives(){emit drivesChanged();}
 Q_INVOKABLE void accept(const QStringList &paths,int filter);
 Q_INVOKABLE void cancel();
 QString m_title,m_kind,m_acceptLabel,m_startFolder,m_currentName;bool m_multiple=false;QVariantList m_filters;int m_currentFilter=0;QStringList m_saveNames;
signals:
 void drivesChanged();
 void finished(bool accepted,const QStringList &paths,int filter);
};

class FileChooserPortal : public QObject, protected QDBusContext {
 Q_OBJECT
 Q_CLASSINFO("D-Bus Interface","org.freedesktop.impl.portal.FileChooser")
 Q_PROPERTY(uint version READ version CONSTANT)
public:
 FileChooserPortal(Backend *backend,SettingsService *settings,QObject *parent=nullptr);
 uint version()const{return 4;}
 bool start();
public slots:
 uint OpenFile(const QDBusObjectPath &handle,const QString &appId,const QString &parentWindow,const QString &title,const QVariantMap &options,QVariantMap &results);
 uint SaveFile(const QDBusObjectPath &handle,const QString &appId,const QString &parentWindow,const QString &title,const QVariantMap &options,QVariantMap &results);
 uint SaveFiles(const QDBusObjectPath &handle,const QString &appId,const QString &parentWindow,const QString &title,const QVariantMap &options,QVariantMap &results);
private:
 void show(const QString &appId,FilePickerSession::Request request,const QVariantMap &options);
 static QString rememberedFolder(const QString &appId);
 static void rememberFolder(const QString &appId,const QString &folder);
 Backend *m_backend;SettingsService *m_settings;QTimer m_idle;int m_open=0;
};

int runFileChooserPortal(Backend *backend,SettingsService *settings);
// `sovel --file-picker-preview [open|save|folder]`: the picker without an app
// asking for it, for trying it out. It prints the chosen paths.
int previewFilePicker(Backend *backend,SettingsService *settings,const QString &kind,const QString &capture);
