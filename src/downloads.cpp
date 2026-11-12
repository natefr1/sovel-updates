#include "downloads.h"
#include "settingsservice.h"
#include "sovelmode.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

namespace {
const QStringList partialSuffixes{".crdownload",".part",".partial",".download",".opdownload"};
}

// ~/.config/user-dirs.dirs is read directly so a change applies at once.
QString Downloads::configuredFolder(){
 QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/user-dirs.dirs");
 if(file.open(QIODevice::ReadOnly)){
  static const QRegularExpression line(R"re(^XDG_DOWNLOAD_DIR="(.*)"\s*$)re");
  while(!file.atEnd()){
   const auto match=line.match(QString::fromUtf8(file.readLine()).trimmed());
   if(!match.hasMatch())continue;
   auto value=match.captured(1);value.replace("$HOME",QDir::homePath());
   if(QFileInfo(value).isDir()&&value!=QDir::homePath())return QDir::cleanPath(value);
  }
 }
 const auto fallback=QDir::homePath()+"/Downloads";
 return QFileInfo(fallback).isDir()?fallback:QDir::homePath();
}

QString Downloads::partialTarget(const QString &name){
 for(const auto &suffix:partialSuffixes)if(name.endsWith(suffix,Qt::CaseInsensitive)&&name.size()>suffix.size())return name.left(name.size()-suffix.size());
 static const QRegularExpression chromium(R"(^\.com\.google\.Chrome\.\w+$|^Unconfirmed \d+\.crdownload$)");
 return chromium.match(name).hasMatch()?name:QString();
}

Downloads::Downloads(SettingsService *settings,bool notify,QObject *parent):QObject(parent),m_settings(settings),m_notify(notify){
 m_debounce.setSingleShot(true);m_debounce.setInterval(350);
 connect(&m_debounce,&QTimer::timeout,this,[this]{scan(true);});
 m_progress.setInterval(1500);
 connect(&m_progress,&QTimer::timeout,this,[this]{scan(true);});
 connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{m_debounce.start();});
 refresh();
}

void Downloads::refresh(){
 const auto next=configuredFolder();
 if(next!=m_folder){
  if(!m_watcher.directories().isEmpty())m_watcher.removePaths(m_watcher.directories());
  m_folder=next;m_known.clear();m_partials.clear();m_scanned=false;
  if(QFileInfo(m_folder).isDir())m_watcher.addPath(m_folder);
 }
 scan(false);
}

bool Downloads::inside(const QString &path)const{
 const auto clean=QFileInfo(path).absoluteFilePath();
 return clean.startsWith(m_folder+"/");
}

void Downloads::scan(bool announce){
 QDir folder(m_folder);
 const auto entries=folder.entryInfoList(QDir::Files|QDir::NoDotAndDotDot,QDir::Time);
 QHash<QString,qint64> known;QHash<QString,QString> partials;QVariantList active,done;
 QSet<QString> inProgress;
 for(const auto &info:entries){const auto target=partialTarget(info.fileName());if(!target.isEmpty())inProgress.insert(target);}
 for(const auto &info:entries){
  const auto name=info.fileName();
  const auto target=partialTarget(name);
  if(!target.isEmpty()){
   partials.insert(name,target);
   active<<QVariantMap{{"name",target==name?QStringLiteral("Download in progress"):target},{"path",info.absoluteFilePath()},{"size",info.size()},{"modified",info.lastModified()},{"partial",true}};
   continue;
  }
  if(name.startsWith('.'))continue;
  // Firefox keeps an empty placeholder with the final name while it writes.
  if(inProgress.contains(name)&&info.size()==0)continue;
  known.insert(name,info.size());
  if(done.size()<40)done<<QVariantMap{{"name",name},{"path",info.absoluteFilePath()},{"size",info.size()},{"modified",info.lastModified()},{"partial",false}};
 }
 if(announce&&m_scanned){
  for(auto it=known.cbegin();it!=known.cend();++it){
   if(m_known.contains(it.key()))continue;
   const auto path=folder.absoluteFilePath(it.key());
   emit completed(it.key(),path);
   const bool wanted=!m_settings||m_settings->preferences().value("downloads.notify",true).toBool();
   if(m_notify&&wanted){
    // Private Mode keeps file names out of notifications.
    const auto body=SovelMode::isPrivate()?QStringLiteral("A file finished downloading."):it.key();
    QProcess::startDetached("notify-send",{"--app-name=Downloads","--icon=folder-download","Download complete",body});
   }
  }
 }
 m_known=known;m_partials=partials;m_scanned=true;
 m_active=active.size();
 m_items=active+done;
 m_status=QFileInfo(m_folder).isDir()?QString():QStringLiteral("The Downloads folder is missing.");
 if(m_active&&!m_progress.isActive())m_progress.start();
 if(!m_active)m_progress.stop();
 emit changed();
}

void Downloads::open(const QString &path){if(inside(path)&&QFileInfo(path).isFile())QDesktopServices::openUrl(QUrl::fromLocalFile(path));}
void Downloads::showInFolder(const QString &path){
 const auto folder=inside(path)?QFileInfo(path).absolutePath():m_folder;
 QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--open-folder",folder});
}
void Downloads::openFolder(){QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--downloads"});}

bool Downloads::setFolder(const QString &value){
 auto path=value;
 if(path.startsWith("file://"))path=QUrl(path).toLocalFile();
 path=QDir::cleanPath(path);
 const QFileInfo info(path);
 if(!info.isDir()||!info.isWritable()||path==QDir::homePath()||!path.startsWith(QDir::homePath()+"/")){m_status="Choose a writable folder inside your home folder.";emit changed();return false;}
 const int code=QProcess::execute("xdg-user-dirs-update",{"--set","DOWNLOAD",path});
 if(code!=0){m_status="The Downloads folder could not be changed.";emit changed();return false;}
 m_status=QString();refresh();
 return true;
}
