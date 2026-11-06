#include "filebrowser.h"
#include <QtConcurrent>
#include <QDir>
#include <QDateTime>
#include <QMimeDatabase>
#include <QStorageInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <algorithm>
#include <QSettings>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>
#include <QImageReader>
#include <QDirIterator>
#include <QElapsedTimer>

namespace {
bool validName(const QString &name){return !name.trimmed().isEmpty()&&name!="."&&name!=".."&&!name.contains('/')&&!name.contains(QChar(0));}
QString sizeText(qint64 bytes){if(bytes<1024)return QString::number(bytes)+" B";double n=bytes;QStringList units{"B","KB","MB","GB","TB"};int i=0;while(n>=1024&&i<4){n/=1024;++i;}return QString::number(n,'f',n<10?1:0)+" "+units[i];}
QString trashFiles(){return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/Trash/files";}
QString collectionName(const QString &path)
{
 if(path=="sovel:recent")return "Recent";
 if(path=="sovel:starred")return "Starred";
 return {};
}
QString helper(const QString &name)
{
 const QString source = QCoreApplication::applicationDirPath()+"/../packaging/"+name;
 return QFileInfo::exists(source) ? source : "/usr/libexec/"+name;
}
bool isArchive(const QString &name)
{
 static const QRegularExpression supported("\\.(zip|7z|rar|tar|gz|tgz|txz|tar\\.xz)$",
                                          QRegularExpression::CaseInsensitiveOption);
 return supported.match(name).hasMatch();
}
QString transferDirectory()
{
 const auto path=qEnvironmentVariable("XDG_STATE_HOME");
 return (path.isEmpty()?QDir::homePath()+"/.local/state":path)+"/Sovel/transfers";
}
bool activeTransfer(const QVariantMap &job)
{
 return QStringList{"queued","running","paused","finishing"}.contains(job.value("state").toString());
}
}
bool FileBrowser::inTrash()const{return QDir::cleanPath(m_path)==QDir::cleanPath(trashFiles());}
QString FileBrowser::collection()const{return collectionName(m_path);}
FileBrowser::FileBrowser(QObject *parent):QObject(parent){
 m_hidden=m_preferences.value("view/hidden",false).toBool();
 m_sort=m_preferences.value("view/sort","name").toString();
 m_debounce.setSingleShot(true);m_debounce.setInterval(180);
 connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this]{m_debounce.start();});
 connect(&m_debounce,&QTimer::timeout,this,&FileBrowser::refresh);
 navigate(QDir::homePath());
 m_volumeTimer.setInterval(3500);
 connect(&m_volumeTimer,&QTimer::timeout,this,&FileBrowser::refreshVolumes);
 m_volumeTimer.start();
 QTimer::singleShot(0,this,&FileBrowser::refreshVolumes);
 m_transferTimer.setInterval(700);
 connect(&m_transferTimer,&QTimer::timeout,this,&FileBrowser::refreshTransfers);
 m_transferTimer.start();
 refreshTransfers();
}
FileBrowser::~FileBrowser(){
 if(m_volumeProbe){m_volumeProbe->disconnect(this);m_volumeProbe->kill();m_volumeProbe->waitForFinished(500);}
 if(m_listing){disconnect(m_listing,nullptr,this,nullptr);m_listing->waitForFinished();}
 if(m_process){disconnect(m_process,nullptr,this,nullptr);m_process->terminate();m_process->waitForFinished(1000);}
}
QVariantList FileBrowser::places()const{
 QVariantList result;
 const QList<QPair<QString,QStandardPaths::StandardLocation>> folders{{"Home",QStandardPaths::HomeLocation},{"Desktop",QStandardPaths::DesktopLocation},{"Documents",QStandardPaths::DocumentsLocation},{"Downloads",QStandardPaths::DownloadLocation},{"Pictures",QStandardPaths::PicturesLocation},{"Music",QStandardPaths::MusicLocation},{"Videos",QStandardPaths::MoviesLocation}};
 for(const auto &folder:folders){
  result<<QVariantMap{{"name",folder.first},{"path",folder.second==QStandardPaths::DownloadLocation?downloadsPath():QStandardPaths::writableLocation(folder.second)},{"icon",folder.first=="Home"?"user-home":"folder-"+folder.first.toLower()}};
  if(folder.first=="Home"){
   result<<QVariantMap{{"name","Recent"},{"path","sovel:recent"},{"icon","document-open-recent"}};
   result<<QVariantMap{{"name","Starred"},{"path","sovel:starred"},{"icon","emblem-favorite"}};
  }
 }
 const auto projects=QDir::home().filePath("Projects");
 result<<QVariantMap{{"name","Projects"},{"path",projects},{"icon","folder-development"}};
 for(const auto &folder:m_preferences.value("navigation/favorites").toStringList())
  if(QFileInfo(folder).isDir())result<<QVariantMap{{"name",QFileInfo(folder).fileName()},{"path",folder},{"icon","emblem-favorite"}};
 result<<QVariantMap{{"name","System Drive"},{"path","/"},{"icon","drive-harddisk"}};
 result<<QVariantMap{{"name","Trash"},{"path",trashFiles()},{"icon","user-trash"}};
 return result;
}
void FileBrowser::navigate(const QString &path){changePath(path,true);}
void FileBrowser::changePath(const QString &path,bool record){
 if(QDir::cleanPath(path)==QDir::cleanPath(trashFiles()))QDir().mkpath(trashFiles());
 const QFileInfo info(path.startsWith("~")?QDir::homePath()+path.mid(1):path);
 if(!info.exists()&&!info.isSymLink()&&info.absoluteFilePath().startsWith(QDir::homePath()+"/")){
  for(const auto &place:places())if(place.toMap().value("path").toString()==info.absoluteFilePath()){QDir().mkpath(info.absoluteFilePath());break;}
 }
 const QFileInfo current(info.absoluteFilePath());
 const bool virtualFolder=!collectionName(path).isEmpty();
 if(!virtualFolder&&(!current.isDir()||!current.isReadable())){m_status="This folder is unavailable or you do not have access.";emit changed();return;}
 const auto next=virtualFolder?path:info.absoluteFilePath();
 if(next!=m_path){
  m_path=next;m_status.clear();m_entries.clear();m_search.clear();
  if(record){while(m_history.size()>m_position+1)m_history.removeLast();m_history<<next;m_position=m_history.size()-1;}
  if(!m_watcher.directories().isEmpty())m_watcher.removePaths(m_watcher.directories());
  if(!virtualFolder)m_watcher.addPath(m_path);
  emit pathChanged();emit tabsChanged();
 }
 refresh();
}
void FileBrowser::back(){if(canBack())changePath(m_history[--m_position],false);}
void FileBrowser::forward(){if(canForward())changePath(m_history[++m_position],false);}
void FileBrowser::up(){navigate(collection().isEmpty()?QFileInfo(m_path).absolutePath():QDir::homePath());}
QVariantList FileBrowser::tabs() const
{
 QVariantList result;
 for(int index=0;index<m_tabs.size();++index){
  const auto &tab=m_tabs[index];
  const auto path=index==m_activeTab?m_path:tab.history.value(tab.position);
  const auto collection=collectionName(path);
  const auto name=!collection.isEmpty()?collection:path==QDir::homePath()?"Home":QFileInfo(path).fileName();
  result.append(QVariantMap{{"path",path},{"name",name.isEmpty()?"System Drive":name}});
 }
 return result;
}
void FileBrowser::newTab()
{
 if(m_tabs.size()>=24)return;
 m_tabs.append(Tab{{m_path},0});
 switchTab(m_tabs.size()-1);
}
void FileBrowser::switchTab(int index)
{
 if(index<0||index>=m_tabs.size()||index==m_activeTab)return;
 m_tabs[m_activeTab]={m_history,m_position};
 m_activeTab=index;
 m_history=m_tabs[index].history;
 m_position=m_tabs[index].position;
 const auto path=m_history.value(m_position,QDir::homePath());
 if(collectionName(path).isEmpty()&&!QFileInfo(path).isDir()){m_history.clear();m_position=-1;navigate(QDir::homePath());}
 else changePath(path,false);
 emit pathChanged();
 emit tabsChanged();
}
void FileBrowser::closeTab(int index)
{
 if(index<0||index>=m_tabs.size()||m_tabs.size()==1)return;
 if(index==m_activeTab)switchTab(index==0?1:index-1);
 m_tabs.removeAt(index);
 if(index<m_activeTab)--m_activeTab;
 emit tabsChanged();
}
QVariantMap FileBrowser::inspect(const QString &path) const
{
 if(!validSelection({path}))return {};
 return inspectFile(path);
}
QVariantMap FileBrowser::inspectFile(const QString &path)
{
 const QFileInfo file(path);
 if(!file.exists()||!file.isReadable())return {};
 const auto mime=QMimeDatabase().mimeTypeForFile(file,QMimeDatabase::MatchExtension);
 QVariantMap result{{"name",file.fileName()},{"path",file.absoluteFilePath()},{"url",QUrl::fromLocalFile(file.absoluteFilePath())},
                    {"mime",mime.name()},{"type",mime.comment()},{"bytes",sizeText(file.size())},
                    {"owner",file.owner()},{"group",file.group()},{"readable",file.isReadable()},
                    {"writable",file.isWritable()},{"symlink",file.isSymLink()?file.symLinkTarget():QString()},
                    {"modified",file.lastModified().toString(Qt::ISODate)}};
 if(file.isFile()&&(mime.inherits("text/plain")||mime.name().startsWith("text/"))){
  QFile contents(path);
  if(contents.open(QIODevice::ReadOnly)){
   const auto raw=contents.read(96*1024);
   result["text"]=QString::fromUtf8(raw);
   result["truncated"]=!contents.atEnd();
   result["encoding"]="UTF-8";
  }
 }
 if(mime.name().startsWith("image/")){
  QImageReader reader(path);
  const auto dimensions=reader.size();
  if(dimensions.isValid())result["dimensions"]=QString("%1 × %2 px").arg(dimensions.width()).arg(dimensions.height());
 }
 return result;
}
void FileBrowser::openLocation(const QUrl &url)
{
 if(!url.isLocalFile())return;
 const QFileInfo file(url.toLocalFile());
 if(file.isDir())navigate(file.absoluteFilePath());
 else if(file.isFile()){
  if(file.suffix().compare("deb",Qt::CaseInsensitive)==0)emit packageRequested(file.absoluteFilePath());
  else if(!QDesktopServices::openUrl(url)){m_status="No app is available to open this file.";emit changed();}
 }
}
void FileBrowser::checksum(const QString &path)
{
 if(validSelection({path})&&QFileInfo(path).isFile())run("sha256sum",{"--",path},"Checksum calculated.");
}
void FileBrowser::searchFolder(const QString &query,const QString &type,int days,int minimumMiB,bool contents)
{
 if(query.size()>300||days<0||days>3650||minimumMiB<0||minimumMiB>1000000||inTrash()||!collection().isEmpty())return;
 m_search={{"query",query.simplified()},{"type",type},{"days",days},{"minimum",qint64(minimumMiB)*1048576},
           {"contents",contents}};
 m_status="Searching this folder and its subfolders…";
 refresh();
}
void FileBrowser::clearSearch()
{
 m_search.clear();m_status.clear();refresh();
}
void FileBrowser::transferTo(const QStringList &paths,const QUrl &folder,bool move)
{
 if(!validSelection(paths)||!folder.isLocalFile()||inTrash()||!QFileInfo(folder.toLocalFile()).isDir())return;
 beginTransfer(paths,folder.toLocalFile(),move);
}
void FileBrowser::shortcut(const QString &path)
{
 if(!validSelection({path})||inTrash()||busy()||!collection().isEmpty())return;
 const auto name=QFileInfo(path).fileName()+" shortcut";
 const auto target=QDir(m_path).filePath(name);
 m_status=QFile::link(path,target)?"Shortcut created.":"Could not create a shortcut. Check for an existing item with that name.";
 refresh();
}
void FileBrowser::setShowHidden(bool value){if(value!=m_hidden){m_hidden=value;m_preferences.setValue("view/hidden",value);refresh();}}
void FileBrowser::setSort(const QString &value){if(value=="name"||value=="modified"||value=="size"||value=="type"){m_sort=value;m_preferences.setValue("view/sort",value);refresh();}}
QString FileBrowser::viewMode() const {return m_preferences.value("view/mode","list").toString();}
void FileBrowser::setViewMode(const QString &mode)
{
 if(!QStringList{"list","grid","compact","gallery"}.contains(mode))return;
 m_preferences.setValue("view/mode",mode);
 emit changed();
}
QString FileBrowser::downloadsPath() const
{
 const auto chosen=m_preferences.value("downloads/folder").toString();
 if(!chosen.isEmpty())return chosen;
 return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}
void FileBrowser::setDownloadsPath(const QUrl &folder)
{
 if(!folder.isLocalFile())return;
 const QFileInfo info(folder.toLocalFile());
 if(!info.isDir()||!info.isWritable()){m_status="Choose an existing, writable Downloads folder.";emit changed();return;}
 run("xdg-user-dirs-update",{"--set","DOWNLOAD",info.canonicalFilePath()},"Downloads location updated.");
}
void FileBrowser::favorite(const QString &path)
{
 if(path!=m_path&&!validSelection({path}))return;
 const QFileInfo info(path);
 if(!info.isDir()||path=="/")return;
 auto favorites=m_preferences.value("navigation/favorites").toStringList();
 if(favorites.contains(path))favorites.removeAll(path);
 else if(favorites.size()<50)favorites.append(path);
 m_preferences.setValue("navigation/favorites",favorites);
 emit changed();
}
void FileBrowser::toggleStar(const QString &path)
{
 if(inTrash()||!validSelection({path}))return;
 auto starred=m_preferences.value("navigation/starred").toStringList();
 if(starred.removeAll(path)==0){
  if(starred.size()>=500){m_status="Starred is full. Remove a star to add another item.";emit changed();return;}
  starred.append(path);
 }
 m_preferences.setValue("navigation/starred",starred);
 refresh();
}
void FileBrowser::recordRecent(const QString &path)
{
 if(inTrash())return;
 auto recent=m_preferences.value("navigation/recent").toStringList();
 recent.removeAll(path);
 recent.prepend(path);
 while(recent.size()>100)recent.removeLast();
 m_preferences.setValue("navigation/recent",recent);
}
void FileBrowser::copyPath(const QString &path)
{
 if(validSelection({path}))QGuiApplication::clipboard()->setText(path);
}
void FileBrowser::refresh(){
 if(m_listing){m_reload=true;return;}
 const auto folder=m_path,sort=m_sort;const bool hidden=m_hidden;const auto search=m_search;
 const auto starred=m_preferences.value("navigation/starred").toStringList();
 const auto recent=m_preferences.value("navigation/recent").toStringList();
 auto *watcher=new QFutureWatcher<QVariantList>(this);m_listing=watcher;emit changed();
 connect(watcher,&QFutureWatcher<QVariantList>::finished,this,[this,watcher,folder,search]{
  if(folder==m_path&&search==m_search){
   m_entries=watcher->result();
   if(!search.isEmpty())m_status=QString("%1 matches · first 500 results from a bounded search").arg(m_entries.size());
   else if(m_status.isEmpty()||m_status.endsWith(" items")||m_status=="Working…")m_status=QString("%1 items").arg(m_entries.size());
  }
  m_listing=nullptr;watcher->deleteLater();emit changed();
  if(m_reload||folder!=m_path){m_reload=false;refresh();}
 });
 watcher->setFuture(QtConcurrent::run([folder,sort,hidden,search,starred,recent]{
  QVariantList entries;QMimeDatabase mime;auto flags=QDir::AllEntries|QDir::NoDotAndDotDot;if(hidden)flags|=QDir::Hidden;
  QFileInfoList infos;
  if(!collectionName(folder).isEmpty()){
   for(const auto &path:folder=="sovel:recent"?recent:starred){
    const QFileInfo file(path);
    if(file.exists()&&(hidden||!file.isHidden()))infos.append(file);
   }
  }
  else if(search.isEmpty())infos=QDir(folder).entryInfoList(flags,QDir::DirsFirst|QDir::Name|QDir::IgnoreCase);
  else {
   QDirIterator iterator(folder,flags|QDir::NoSymLinks,QDirIterator::Subdirectories);
   QElapsedTimer timer;timer.start();
   int visited=0;
   const auto query=search.value("query").toString();
   const auto type=search.value("type").toString();
   const auto days=search.value("days").toInt();
   const auto minimum=search.value("minimum").toLongLong();
   const auto cutoff=QDateTime::currentDateTime().addDays(-days);
   while(iterator.hasNext()&&++visited<=25000&&timer.elapsed()<3500&&infos.size()<500){
    iterator.next();
    const auto file=iterator.fileInfo();
    if(days>0&&file.lastModified()<cutoff)continue;
    if(minimum>0&&(file.isDir()||file.size()<minimum))continue;
    const auto media=mime.mimeTypeForFile(file,QMimeDatabase::MatchExtension).name();
    if(type=="folder"&&!file.isDir())continue;
    if(type=="archive"&&!isArchive(file.fileName()))continue;
    if(type.endsWith('/')&&!media.startsWith(type))continue;
    bool match=query.isEmpty()||file.fileName().contains(query,Qt::CaseInsensitive);
    if(!match&&search.value("contents").toBool()&&media.startsWith("text/")&&file.isFile()){
     QFile sample(file.absoluteFilePath());
     if(sample.open(QIODevice::ReadOnly))match=QString::fromUtf8(sample.read(64*1024)).contains(query,Qt::CaseInsensitive);
    }
    if(match)infos.append(file);
   }
  }
  if(folder!="sovel:recent"||sort!="name")std::stable_sort(infos.begin(),infos.end(),[&](const QFileInfo &a,const QFileInfo &b){
   if(a.isDir()!=b.isDir())return a.isDir();
   if(sort=="modified"&&a.lastModified()!=b.lastModified())return a.lastModified()>b.lastModified();
   if(sort=="size"&&a.size()!=b.size())return a.size()>b.size();
   if(sort=="type"&&a.suffix()!=b.suffix())return a.suffix().compare(b.suffix(),Qt::CaseInsensitive)<0;
   return QString::localeAwareCompare(a.fileName(),b.fileName())<0;
  });
  for(const auto &file:infos){
   const auto type=mime.mimeTypeForFile(file,QMimeDatabase::MatchExtension);
   entries<<QVariantMap{{"name",file.fileName()},{"path",file.absoluteFilePath()},{"url",QUrl::fromLocalFile(file.absoluteFilePath())},{"folder",file.isDir()},{"size",file.size()},{"sizeText",file.isDir()?QString("—"):sizeText(file.size())},{"modified",file.lastModified().toString("dd MMM yyyy, HH:mm")},{"type",file.isDir()?QString("Folder"):type.comment()},{"mime",type.name()},{"archive",file.isFile()&&isArchive(file.fileName())},{"icon","image://apps/"+QString::fromLatin1(QUrl::toPercentEncoding(file.isDir()?"folder":type.iconName()))},{"package",file.suffix().compare("deb",Qt::CaseInsensitive)==0},{"starred",starred.contains(file.absoluteFilePath())}};
  }
  return entries;
 }));
}
bool FileBrowser::validSelection(const QStringList &paths)const{
 if(paths.isEmpty()||paths.size()>1000)return false;
 for(const auto &path:paths){bool found=false;for(const auto &entry:m_entries)if(entry.toMap().value("path").toString()==path){found=true;break;}if(!found)return false;}
 return true;
}
void FileBrowser::createFolder(const QString &name){if(busy()||inTrash()||!collection().isEmpty())return;if(!validName(name)){m_status="Choose a folder name without slashes.";emit changed();return;}m_status=QDir(m_path).mkdir(name)?"Folder created.":"Could not create the folder. Check the name and permissions.";refresh();}
void FileBrowser::rename(const QString &path,const QString &name){
 if(busy()||inTrash()||!validSelection({path}))return;
 if(!validName(name)){m_status="Choose a name without slashes.";emit changed();return;}
 const auto destination=QDir(QFileInfo(path).absolutePath()).filePath(name);
 if(QFileInfo::exists(destination)||QFileInfo(destination).isSymLink()){m_status="An item with that name already exists.";emit changed();return;}
 const bool renamed=QDir().rename(path,destination);
 if(renamed){
  for(const auto &key:{"navigation/starred","navigation/recent","navigation/favorites"}){
   auto paths=m_preferences.value(key).toStringList();
   for(auto &item:paths)if(item==path||item.startsWith(path+"/"))item=destination+item.mid(path.size());
   m_preferences.setValue(key,paths);
  }
 }
 m_status=renamed?"Item renamed.":"The item could not be renamed.";refresh();
}
void FileBrowser::trash(const QStringList &paths){
 if(busy()||inTrash()||!validSelection(paths))return;run("gio",QStringList{"trash","--"}+paths,"Moved to Trash.");
}
void FileBrowser::restore(const QString &path){
 if(busy()||!inTrash()||!validSelection({path}))return;
 const auto infoPath=QDir(trashFiles()).absoluteFilePath("../info/"+QFileInfo(path).fileName()+".trashinfo");QSettings info(infoPath,QSettings::IniFormat);
 const auto destination=QUrl::fromPercentEncoding(info.value("Trash Info/Path").toByteArray());
 if(!QFileInfo(destination).isAbsolute()||destination.contains(QChar(0))){m_status="The original location is missing from this Trash item.";emit changed();return;}
 if(QFileInfo::exists(destination)||QFileInfo(destination).isSymLink()){m_status="An item already exists at the original location. Copy this item to another folder instead.";emit changed();return;}
 if(!QDir().mkpath(QFileInfo(destination).absolutePath())){m_status="The original folder could not be recreated.";emit changed();return;}
 run("mv",{"--no-clobber","--",path,destination},"Item restored.",infoPath);
}
void FileBrowser::copy(const QStringList &paths,bool cut){
 if(!validSelection(paths)||inTrash()&&cut)return;
 auto *data=new QMimeData;QList<QUrl> urls;QByteArray gnome=cut?"cut":"copy";
 for(const auto &path:paths){const auto url=QUrl::fromLocalFile(path);urls<<url;gnome+='\n'+url.toEncoded();}
 data->setUrls(urls);data->setData("x-special/gnome-copied-files",gnome);data->setData("application/x-kde-cutselection",cut?"1":"0");QGuiApplication::clipboard()->setMimeData(data);
 m_status=QString("%1 items ready to %2").arg(paths.size()).arg(cut?"move":"copy");emit changed();
}
void FileBrowser::paste(){
 if(busy()||inTrash()||!collection().isEmpty())return;const auto *data=QGuiApplication::clipboard()->mimeData();if(!data||!data->hasUrls())return;
 QStringList paths;for(const auto &url:data->urls()){
  if(!url.isLocalFile()){m_status="Only files on this computer can be pasted here.";emit changed();return;}
  const QFileInfo info(url.toLocalFile());const auto source=info.absoluteFilePath();const auto target=QDir(m_path).filePath(info.fileName());
  if(!info.exists()&&!info.isSymLink()){m_status="A copied item is no longer available.";emit changed();return;}
  if(QFileInfo::exists(target)||QFileInfo(target).isSymLink()){m_status="An item named “"+info.fileName()+"” already exists. Rename it before pasting.";emit changed();return;}
  if(info.isDir()&&!info.isSymLink()&&(QDir(m_path).canonicalPath()==info.canonicalFilePath()||QDir(m_path).canonicalPath().startsWith(info.canonicalFilePath()+"/"))){m_status="A folder cannot be pasted inside itself.";emit changed();return;}
  paths<<source;
 }
 if(paths.isEmpty())return;
 const bool cut=data->data("application/x-kde-cutselection")=="1"||data->data("x-special/gnome-copied-files").startsWith("cut\n");
 beginTransfer(paths,m_path,cut);
}
void FileBrowser::beginTransfer(const QStringList &paths,const QString &destination,bool move)
{
 enqueueTransfer({{"sources",QJsonArray::fromStringList(paths)},{"destination",destination},{"move",move}});
}
void FileBrowser::setOrganizeDownloads(bool enabled)
{
 m_preferences.setValue("downloads/organize",enabled);
 emit changed();
}
void FileBrowser::setNotifyDownloads(bool enabled)
{
 m_preferences.setValue("downloads/notify",enabled);
 emit changed();
}
QUrl FileBrowser::lastDownloadFolder()const
{
 const auto previous=m_preferences.value("downloads/lastFolder").toString();
 return QUrl::fromLocalFile(QFileInfo(previous).isDir()?previous:downloadsPath());
}
void FileBrowser::download(const QString &url,const QString &filename,const QUrl &chosen)
{
 const QUrl address(url,QUrl::StrictMode);
 if(!address.isValid()||address.scheme()!="https"||address.host().isEmpty()||!address.userInfo().isEmpty()||
    !validName(filename)||filename.toUtf8().size()>240){
  m_status="Enter an HTTPS address and a filename without slashes.";emit changed();return;
 }
 auto folder=downloadsPath();
 if(!chosen.isEmpty()){
  const QFileInfo selected(chosen.toLocalFile());
  if(!chosen.isLocalFile()||!selected.isDir()||!selected.isWritable()){
   m_status="Choose a writable local folder for this download.";emit changed();return;
  }
  folder=selected.canonicalFilePath();
  m_preferences.setValue("downloads/lastFolder",folder);
 }else if(!folder.isEmpty()&&QDir().mkpath(folder)&&organizeDownloads()){
  const auto mime=QMimeDatabase().mimeTypeForFile(filename,QMimeDatabase::MatchExtension).name();
  const auto suffix=QFileInfo(filename).suffix().toLower();
  QString category="Other";
  if(QStringList{"deb","rpm","appimage","iso","msi","exe"}.contains(suffix))category="Installers";
  else if(isArchive(filename))category="Archives";
  else if(mime.startsWith("image/"))category="Images";
  else if(mime.startsWith("video/"))category="Videos";
  else if(mime.startsWith("audio/"))category="Audio";
  else if(mime.startsWith("text/")||mime.contains("document")||mime=="application/pdf")category="Documents";
  const QFileInfo organized(QDir(folder).filePath(category));
  if(organized.isSymLink()||!QDir().mkpath(organized.absoluteFilePath())){
   m_status="The download category is unavailable. Choose another folder or turn off organization.";emit changed();return;
  }
  folder=organized.absoluteFilePath();
 }
 if(folder.isEmpty()||!QDir().mkpath(folder)){m_status="The Downloads folder is unavailable.";emit changed();return;}
 enqueueTransfer({{"kind","download"},{"url",address.toString(QUrl::FullyEncoded)},{"filename",filename},
                  {"destination",folder},{"notify",notifyDownloads()}});
}
void FileBrowser::enqueueTransfer(QJsonObject job)
{
 const auto directory=transferDirectory();
 if(!QDir().mkpath(directory)){m_status="The transfer queue could not be opened.";emit changed();return;}
 const auto identifier=QUuid::createUuid().toString(QUuid::Id128);
 QSaveFile request(directory+"/"+identifier+".json");
 job["id"]=identifier;
 job["state"]="queued";
 job["created"]=QDateTime::currentSecsSinceEpoch();
 const auto document=QJsonDocument(job).toJson(QJsonDocument::Compact);
 if(!request.open(QIODevice::WriteOnly)||!request.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner)||
    request.write(document)!=document.size()||!request.commit()){
  m_status="The transfer job could not be saved.";emit changed();return;
 }
 if(!QProcess::startDetached("python3",{helper("sovel-transfer"),identifier})){
  QFile::remove(directory+"/"+identifier+".json");
  m_status="The transfer service could not start.";
 }else if(job.value("kind")=="download")m_status="Download added to Download Manager.";
 else m_status=job.value("move").toBool()?"Move added to Transfers.":"Copy added to Transfers.";
 refreshTransfers();
 emit changed();
}
void FileBrowser::refreshTransfers()
{
 QDir queue(transferDirectory());
 QVariantList result;
 for(const auto &entry:queue.entryInfoList({"*.json"},QDir::Files|QDir::NoSymLinks,QDir::Time).mid(0,100)){
  QFile file(entry.absoluteFilePath());
  if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024)continue;
  auto job=QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
  if(job.isEmpty())continue;
  const auto state=job.value("state").toString();
  if(activeTransfer(job)){
   const auto pid=job.value("pid").toLongLong();
   const auto age=QDateTime::currentSecsSinceEpoch()-job.value("created").toLongLong();
   if((pid>0&&!QFileInfo::exists("/proc/"+QString::number(pid)))||(state=="queued"&&age>120)){
    job["state"]="interrupted";job["error"]="The transfer worker exited before finishing.";
   }
  }
  result.append(job);
 }
 if(result==m_transfers)return;
 m_transfers=result;emit transfersChanged();
}
void FileBrowser::transferAction(const QString &id,const QString &action)
{
 static const QRegularExpression identifier("^[a-f0-9]{32}$");
 if(!identifier.match(id).hasMatch())return;
 QVariantMap job;
 for(const auto &entry:m_transfers)if(entry.toMap().value("id")==id){job=entry.toMap();break;}
 if(job.isEmpty())return;
 const auto state=job.value("state").toString();
 if(action=="open"){navigate(job.value("destination").toString());return;}
 if(action=="open-file"&&state=="complete"&&job.value("kind")=="download"){
  const auto path=job.value("file").toString();
  const QFileInfo file(path);
  if(!file.isFile()||file.absolutePath()!=QFileInfo(job.value("destination").toString()).absoluteFilePath())return;
  if(file.suffix().compare("deb",Qt::CaseInsensitive)==0)emit packageRequested(path);
  else QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  return;
 }
 if(action=="clear"&&!activeTransfer(job)){QFile::remove(transferDirectory()+"/"+id+".json");refreshTransfers();return;}
 if(action=="retry"&&QStringList{"failed","cancelled","interrupted"}.contains(state)){
  if(job.value("kind")=="download"){download(job.value("url").toString(),job.value("filename").toString(),QUrl::fromLocalFile(job.value("destination").toString()));return;}
  beginTransfer(job.value("sources").toStringList(),job.value("destination").toString(),job.value("move").toBool());return;
 }
 if(state=="finishing"||!activeTransfer(job)||!QStringList{"pause","resume","cancel"}.contains(action))return;
 QSaveFile control(transferDirectory()+"/"+id+".control");
 if(control.open(QIODevice::WriteOnly)){control.write(action.toUtf8());control.commit();}
}
void FileBrowser::duplicate(const QString &path)
{
 if(!validSelection({path})||inTrash()||busy()||!collection().isEmpty())return;
 const QFileInfo info(path);
 const auto base=info.isDir()?info.fileName():info.completeBaseName();
 const auto suffix=info.isDir()||info.suffix().isEmpty()?QString():"."+info.suffix();
 QString target=QDir(m_path).filePath(base+" copy"+suffix);
 for(int index=2;QFileInfo::exists(target)||QFileInfo(target).isSymLink();++index){
  if(index>1000){m_status="Too many files share that name.";emit changed();return;}
  target=QDir(m_path).filePath(base+" copy "+QString::number(index)+suffix);
 }
 run("cp",{"-a","--no-clobber","--",path,target},"Item duplicated.");
}
void FileBrowser::openResultLocation()
{
 if(m_archiveDestination.isEmpty())return;
 const QFileInfo result(m_archiveDestination);
 navigate(result.isDir()?result.absoluteFilePath():result.absolutePath());
}
void FileBrowser::setAutoMount(bool enabled)
{
 if(enabled==autoMount())return;
 m_preferences.setValue("drives/autoMount",enabled);
 m_autoMountAttempted.clear();
 emit changed();
 if(enabled)refreshVolumes();
}
void FileBrowser::refreshVolumes()
{
 if(m_volumeProbe)return;
 auto *probe=new QProcess(this);
 m_volumeProbe=probe;
 connect(probe,&QProcess::errorOccurred,this,[this,probe](QProcess::ProcessError error){
  if(error!=QProcess::FailedToStart)return;
  m_volumeProbe=nullptr;
  m_volumeError="Drive discovery could not start.";
  probe->deleteLater();
  emit changed();
 });
 connect(probe,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,probe](int code,QProcess::ExitStatus exit){
  m_volumeProbe=nullptr;
  const auto result=QJsonDocument::fromJson(probe->readAllStandardOutput()).object();
  const auto volumes=result.value("volumes").toArray().toVariantList();
  const auto error=code==0&&exit==QProcess::NormalExit&&result.contains("volumes")?QString():
    QString("Drive discovery is unavailable. ")+QString::fromUtf8(probe->readAllStandardError()).left(240);
  if(volumes!=m_volumes||error!=m_volumeError){m_volumes=volumes;m_volumeError=error;emit changed();}
  probe->deleteLater();
  if(!error.isEmpty()||!autoMount())return;
  QSet<QString> connected;
  for(const auto &entry:volumes){
   const auto volume=entry.toMap();
   const auto identity=volume.value("identity").toString();
   if(volume.value("connection")!="usb"||!volume.value("mountable").toBool()||
      volume.value("protected").toBool()||identity.isEmpty())continue;
   const auto id=volume.value("id").toString();
   const auto key=id+"|"+identity;
   connected.insert(key);
   if(!volume.value("mount").toString().isEmpty()){
    m_autoMountAttempted.insert(key);
    continue;
   }
   if(busy()||m_autoMountAttempted.contains(key))continue;
   m_autoMountAttempted.insert(key);
   run("python3",{helper("sovel-volumes"),"auto-mount",id,identity},"USB drive mounted.");
  }
  m_autoMountAttempted.intersect(connected);
 });
 QTimer::singleShot(10000,probe,[probe]{if(probe->state()!=QProcess::NotRunning)probe->kill();});
 probe->start("python3",{helper("sovel-volumes"),"list"});
}
void FileBrowser::volumeAction(const QString &action,const QString &id,const QString &label)
{
 if(busy()||!QStringList{"open","mount","unmount","eject","check","label"}.contains(action))return;
 QVariantMap volume;
 for(const auto &item:m_volumes)if(item.toMap().value("id")==id){volume=item.toMap();break;}
 if(volume.isEmpty())return;
 if(action=="unmount"||action=="eject"){
  refreshTransfers();
  QStringList mounts;
  if(action=="eject"){
   for(const auto &entry:m_volumes){
    const auto item=entry.toMap();
    if(item.value("drive")==volume.value("drive"))mounts+=item.value("mounts").toStringList();
   }
  }else mounts=volume.value("mounts").toStringList();
  for(const auto &item:m_transfers){
   const auto job=item.toMap();
   if(!activeTransfer(job))continue;
   auto locations=job.value("sources").toStringList();
   locations<<job.value("destination").toString();
   for(const auto &location:locations)for(const auto &root:mounts){
    if(!root.isEmpty()&&(location==root||location.startsWith(root+"/"))){
     m_status="Wait for transfers on this drive before removing it.";emit changed();return;
    }
   }
  }
 }
 if(action=="open"&&!volume.value("mount").toString().isEmpty()){
  navigate(volume.value("mount").toString());
  return;
 }
 QStringList args{helper("sovel-volumes"),action=="open"?"mount":action,id};
 if(action=="label")args.append(label);
 run("python3",args,"Drive operation complete.");
}
void FileBrowser::formatVolume(const QString &id,const QString &filesystem,const QString &confirmation,const QString &identity)
{
 if(busy()||identity.isEmpty())return;
 for(const auto &entry:m_volumes){
  const auto volume=entry.toMap();
  if(volume.value("id")!=id||volume.value("protected").toBool()||volume.value("readonly").toBool()||
     !volume.value("mount").toString().isEmpty()||confirmation!=volume.value("device").toString()||identity!=volume.value("identity").toString())continue;
  run("python3",{helper("sovel-volumes"),"format",id,filesystem,confirmation,identity},"Volume formatted.");
  return;
 }
}
void FileBrowser::archiveAction(const QString &action,const QString &path,const QString &destination,bool newFolder,const QString &password)
{
 if(busy()||!validSelection({path})||!isArchive(path)||!QFileInfo(path).isFile())return;
 if(!QStringList{"preview","test","extract"}.contains(action)||password.contains('\n')||password.size()>4096)return;
 QStringList args{helper("sovel-archive"),action,path};
 if(action=="extract"){
  if(!QFileInfo(destination).isAbsolute()||!QFileInfo(destination).isDir())return;
  args<<destination<<(newFolder?"new":"here");
 }
 m_archiveEntries.clear();m_archiveDestination.clear();
 run("python3",args,action=="test"?"Archive test passed.":action=="preview"?"Archive contents loaded.":"Extraction complete.",{},password.toUtf8()+"\n");
}
void FileBrowser::compress(const QStringList &paths,const QString &destination,int level)
{
 if(busy()||inTrash()||!validSelection(paths)||!QFileInfo(destination).isAbsolute())return;
 m_archiveDestination.clear();
 run("python3",QStringList{helper("sovel-archive"),"create",destination,QString::number(level)}+paths,
     "Archive created.");
}
void FileBrowser::readOperationOutput(QProcess *process)
{
 m_operationOutput+=process->readAllStandardOutput();
 if(process->program()=="sha256sum")return;
 while(m_operationOutput.contains('\n')){
  const auto end=m_operationOutput.indexOf('\n');
  const auto result=QJsonDocument::fromJson(m_operationOutput.left(end)).object();
  m_operationOutput.remove(0,end+1);
  if(result.contains("done")){
   m_operationProgress=QString("%1 entries · %2").arg(result.value("done").toInt())
       .arg(sizeText(result.value("bytes").toInteger()));
   emit changed();
  }else{
   for(auto item=result.begin();item!=result.end();++item)m_operationResult.insert(item.key(),item.value());
  }
 }
 if(m_operationOutput.size()>4*1024*1024)process->kill();
}
void FileBrowser::finishOperationOutput(const QString &program,const QStringList &args)
{
 if(program=="xdg-user-dirs-update"){
  m_preferences.setValue("downloads/folder",args.value(2));
  m_preferences.sync();
  return;
 }
 if(program=="sha256sum"){
  const auto digest=QString::fromLatin1(m_operationOutput).trimmed().remove(QRegularExpression("^\\\\")).left(64);
  if(QRegularExpression("^[a-f0-9]{64}$").match(digest).hasMatch()){
   QGuiApplication::clipboard()->setText(digest);
   m_status="SHA-256 copied to the clipboard.";
  }
  return;
 }
 if(program!="python3")return;
 if(args.value(0).endsWith("/sovel-volumes")){
  const auto mount=m_operationResult.value("mount").toString();
  if(!mount.isEmpty()&&args.value(1)!="auto-mount")navigate(mount);
  else if((args.value(1)=="unmount"||args.value(1)=="eject")&&collection().isEmpty()&&!QFileInfo(m_path).isDir())navigate(QDir::homePath());
  refreshVolumes();
 }
 if(args.value(0).endsWith("/sovel-archive")){
  m_archiveEntries=m_operationResult.value("entries").toArray().toVariantList();
  m_archiveDestination=m_operationResult.value("destination").toString();
  if(m_operationResult.value("truncated").toBool())m_status="Showing the first 500 entries.";
 }
}
void FileBrowser::run(const QString &program,const QStringList &args,const QString &success,const QString &removeMetadata,const QByteArray &input){
 if(busy())return;auto *process=new QProcess(this);m_process=process;m_status="Working…";emit changed();
 m_operationOutput.clear();m_operationResult={};m_operationProgress.clear();
 connect(process,&QProcess::started,this,[process,input]{if(!input.isEmpty())process->write(input);process->closeWriteChannel();});
 connect(process,&QProcess::readyReadStandardOutput,this,[this,process]{readOperationOutput(process);});
 connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){if(error==QProcess::FailedToStart){m_process=nullptr;m_status="The file operation could not start.";process->deleteLater();emit changed();}});
 connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,program,success,removeMetadata,args](int code,QProcess::ExitStatus exit){readOperationOutput(process);m_process=nullptr;bool ok=code==0&&exit==QProcess::NormalExit;if(ok&&!removeMetadata.isEmpty()){const QFileInfo original(args.value(args.size()-2));ok=!original.exists()&&!original.isSymLink();if(ok)QFile::remove(removeMetadata);}m_status=ok?m_operationResult.value("message").toString(success):QString::fromUtf8(process->readAllStandardError()).trimmed().left(1500);if(m_status.isEmpty())m_status="Some items could not be changed.";if(ok)finishOperationOutput(program,args);process->deleteLater();emit changed();refresh();});
 process->start(program,args);
}
void FileBrowser::open(const QString &path){
 if(!validSelection({path}))return;const QFileInfo info(path);
 if(!info.exists())return;
 recordRecent(path);
 if(info.isDir())navigate(path);
 else if(info.suffix().compare("deb",Qt::CaseInsensitive)==0)emit packageRequested(path);
 else if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path))){m_status="No app is available to open this file.";emit changed();}
}
