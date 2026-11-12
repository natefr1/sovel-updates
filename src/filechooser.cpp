#include "filechooser.h"
#include "backend.h"
#include "settingsservice.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QQmlContext>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <cstdio>

namespace {
QString home(){return QDir::homePath();}
QString location(QStandardPaths::StandardLocation which){
 const auto path=QStandardPaths::writableLocation(which);
 return path.isEmpty()||path==home()?QString():path;
}
QString configFile(){return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/Sovel/file-picker.json";}
QString bytesPath(const QVariant &value){
 // Portal paths arrive as NUL-terminated byte arrays.
 auto bytes=value.toByteArray();
 while(bytes.endsWith('\0'))bytes.chop(1);
 return QString::fromUtf8(bytes);
}
// a(sa(us)): [(name,[(0 glob | 1 mime, pattern)])]
QVariantMap readFilter(const QDBusArgument &argument){
 QString name;QStringList patterns;QMimeDatabase mimes;
 argument.beginStructure();argument>>name;
 argument.beginArray();
 while(!argument.atEnd()){
  uint type=0;QString pattern;
  argument.beginStructure();argument>>type>>pattern;argument.endStructure();
  if(type==0)patterns<<pattern;
  else for(const auto &glob:mimes.mimeTypeForName(pattern).globPatterns())patterns<<glob;
 }
 argument.endArray();argument.endStructure();
 patterns.removeDuplicates();
 return {{"name",name.isEmpty()?patterns.join(", "):name},{"patterns",patterns.isEmpty()?QStringList{"*"}:patterns}};
}
QVariantList readFilters(const QVariant &value){
 QVariantList filters;
 if(!value.canConvert<QDBusArgument>())return filters;
 const auto argument=value.value<QDBusArgument>();
 argument.beginArray();
 while(!argument.atEnd())filters<<readFilter(argument);
 argument.endArray();
 return filters;
}
QString readFilterName(const QVariant &value){
 if(!value.canConvert<QDBusArgument>())return {};
 return readFilter(value.value<QDBusArgument>()).value("name").toString();
}
}

FilePickerSession::FilePickerSession(const Request &request,QObject *parent):QObject(parent),
 m_title(request.title),m_kind(request.kind),m_acceptLabel(request.acceptLabel),m_startFolder(request.startFolder),
 m_currentName(request.currentName),m_multiple(request.multiple),m_filters(request.filters),m_saveNames(request.saveNames){
 if(m_kind!="folder"&&m_kind!="save-files"&&!m_filters.isEmpty()){
  for(int i=0;i<m_filters.size();++i)if(m_filters.at(i).toMap().value("name").toString()==request.currentFilterName){m_currentFilter=i;break;}
 }
 if(m_startFolder.isEmpty()||!QFileInfo(m_startFolder).isDir())m_startFolder=home();
}

QVariantList FilePickerSession::places()const{
 QVariantList result{QVariantMap{{"name","Recent"},{"path","recent:"},{"symbol","clock"}},QVariantMap{{"name","Home"},{"path",home()},{"symbol","home"}}};
 const QList<QPair<QString,QStandardPaths::StandardLocation>> named{{"Desktop",QStandardPaths::DesktopLocation},{"Documents",QStandardPaths::DocumentsLocation},
  {"Downloads",QStandardPaths::DownloadLocation},{"Pictures",QStandardPaths::PicturesLocation},{"Videos",QStandardPaths::MoviesLocation},{"Music",QStandardPaths::MusicLocation}};
 for(const auto &[name,which]:named){
  const auto path=location(which);
  if(!path.isEmpty()&&QFileInfo(path).isDir())result<<QVariantMap{{"name",name},{"path",path},{"symbol",name=="Downloads"?"downloads":name=="Pictures"?"wallpaper":"storage"}};
 }
 // GTK bookmarks, so folders pinned in other file managers appear here too.
 QFile bookmarks(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/gtk-3.0/bookmarks");
 if(bookmarks.open(QIODevice::ReadOnly)){
  int count=0;
  while(!bookmarks.atEnd()&&count<12){
   const auto line=QString::fromUtf8(bookmarks.readLine()).trimmed();
   const auto space=line.indexOf(' ');
   const QUrl url(space<0?line:line.left(space));
   if(!url.isLocalFile()||!QFileInfo(url.toLocalFile()).isDir())continue;
   const auto label=space<0?QFileInfo(url.toLocalFile()).fileName():line.mid(space+1);
   result<<QVariantMap{{"name",label},{"path",url.toLocalFile()},{"symbol","storage"}};++count;
  }
 }
 return result;
}

QVariantList FilePickerSession::drives()const{
 QVariantList result;
 for(const auto &volume:QStorageInfo::mountedVolumes()){
  const auto root=volume.rootPath();
  if(!volume.isValid()||!volume.isReady())continue;
  if(!root.startsWith("/media/")&&!root.startsWith("/run/media/")&&!root.startsWith("/mnt/"))continue;
  auto name=volume.displayName();
  if(name.isEmpty()||name==root)name=QFileInfo(root).fileName();
  result<<QVariantMap{{"name",name},{"path",root},{"symbol","storage"},{"free",volume.bytesAvailable()}};
 }
 return result;
}

QVariantList FilePickerSession::recent()const{
 QFile file(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)+"/recently-used.xbel");
 QVariantList result;
 if(file.size()>8*1024*1024||!file.open(QIODevice::ReadOnly))return result;
 const auto text=QString::fromUtf8(file.readAll());
 static const QRegularExpression href(R"re(href="(file://[^"]+)")re");
 QStringList paths;
 for(auto match=href.globalMatch(text);match.hasNext();){
  const auto path=QUrl(match.next().captured(1)).toLocalFile();
  paths.removeAll(path);paths.prepend(path);
 }
 for(const auto &path:paths){
  const QFileInfo info(path);
  if(!info.exists())continue;
  result<<QVariantMap{{"name",info.fileName()},{"path",info.absoluteFilePath()},{"folder",info.isDir()},{"size",info.size()},{"modified",info.lastModified()}};
  if(result.size()>=40)break;
 }
 return result;
}

bool FilePickerSession::exists(const QString &path)const{return QFileInfo::exists(path);}
bool FilePickerSession::isFolder(const QString &path)const{return QFileInfo(path).isDir();}
QString FilePickerSession::parentOf(const QString &path)const{const auto parent=QFileInfo(path).absolutePath();return parent==path?path:parent;}
QString FilePickerSession::join(const QString &folder,const QString &name)const{return QDir(folder).absoluteFilePath(name);}
QString FilePickerSession::expand(const QString &typed,const QString &folder)const{
 auto value=typed.trimmed();
 if(value=="~")return home();
 if(value.startsWith("~/"))value=home()+value.mid(1);
 if(value.startsWith("file://"))value=QUrl(value).toLocalFile();
 return QDir::cleanPath(QDir::isAbsolutePath(value)?value:QDir(folder).absoluteFilePath(value));
}
QString FilePickerSession::displayPath(const QString &path)const{
 if(path=="recent:")return "Recent";
 return path==home()?QStringLiteral("Home"):path.startsWith(home()+"/")?"~"+path.mid(home().size()):path;
}
QString FilePickerSession::makeFolder(const QString &folder,const QString &name){
 const auto clean=name.trimmed();
 if(clean.isEmpty()||clean.contains('/')||clean=="."||clean=="..")return "Choose a folder name without slashes.";
 const auto path=QDir(folder).absoluteFilePath(clean);
 if(QFileInfo::exists(path))return "Something with this name already exists here.";
 return QDir().mkdir(path)?QString():"This folder could not be created here.";
}
void FilePickerSession::accept(const QStringList &paths,int filter){emit finished(true,paths,filter);}
void FilePickerSession::cancel(){emit finished(false,{},-1);}

FileChooserPortal::FileChooserPortal(Backend *backend,SettingsService *settings,QObject *parent):QObject(parent),m_backend(backend),m_settings(settings){
 // Stay available briefly between requests, then exit; D-Bus starts it again.
 m_idle.setSingleShot(true);m_idle.setInterval(10*60*1000);
 connect(&m_idle,&QTimer::timeout,qApp,[this]{if(!m_open)QCoreApplication::quit();});
 m_idle.start();
}

bool FileChooserPortal::start(){
 auto bus=QDBusConnection::sessionBus();
 if(!bus.registerObject("/org/freedesktop/portal/desktop",this,QDBusConnection::ExportAllSlots|QDBusConnection::ExportAllProperties))return false;
 return bus.registerService("org.freedesktop.impl.portal.desktop.sovel");
}

QString FileChooserPortal::rememberedFolder(const QString &appId){
 QFile file(configFile());
 if(appId.isEmpty()||!file.open(QIODevice::ReadOnly)||file.size()>256*1024)return {};
 const auto folder=QJsonDocument::fromJson(file.readAll()).object().value(appId).toString();
 return QFileInfo(folder).isDir()?folder:QString();
}

void FileChooserPortal::rememberFolder(const QString &appId,const QString &folder){
 if(appId.isEmpty()||folder.isEmpty())return;
 QFile file(configFile());
 QJsonObject saved;
 if(file.open(QIODevice::ReadOnly)&&file.size()<256*1024)saved=QJsonDocument::fromJson(file.readAll()).object();
 file.close();
 saved.insert(appId,folder);
 while(saved.size()>100)saved.remove(saved.keys().first());
 QDir().mkpath(QFileInfo(configFile()).absolutePath());
 QSaveFile out(configFile());
 if(out.open(QIODevice::WriteOnly)){out.write(QJsonDocument(saved).toJson(QJsonDocument::Compact));out.commit();}
}

void FileChooserPortal::show(const QString &appId,FilePickerSession::Request request,const QVariantMap &options){
 auto reply=message();
 const auto asked=bytesPath(options.value("current_folder"));
 const bool remember=m_settings->preferences().value("downloads.rememberFolder",true).toBool();
 const auto remembered=remember?rememberedFolder(appId):QString();
 const bool saving=request.kind=="save"||request.kind=="save-files";
 const auto downloads=location(QStandardPaths::DownloadLocation);
 request.startFolder=!asked.isEmpty()&&QFileInfo(asked).isDir()?asked:!remembered.isEmpty()?remembered:saving&&!downloads.isEmpty()?downloads:home();
 if(request.kind=="save"){
  const auto existing=bytesPath(options.value("current_file"));
  if(!existing.isEmpty()){request.startFolder=QFileInfo(existing).absolutePath();if(request.currentName.isEmpty())request.currentName=QFileInfo(existing).fileName();}
 }
 if(request.acceptLabel.isEmpty())request.acceptLabel=request.kind=="open"?"Open":request.kind=="folder"?"Select Folder":"Save";
 if(request.title.isEmpty())request.title=request.kind=="open"?"Open File":request.kind=="folder"?"Select Folder":request.kind=="save-files"?"Choose Where to Save":"Save File";

 auto *session=new FilePickerSession(request,this);
 auto *view=new QQuickView();
 view->setTitle(request.title);
 view->setColor(QColor(m_settings->colors().value("background").toString()));
 view->setResizeMode(QQuickView::SizeRootObjectToView);
 view->setMinimumSize(QSize(640,420));
 view->resize(980,640);
 view->rootContext()->setContextProperty("Picker",session);
 view->rootContext()->setContextProperty("SettingsService",m_settings);
 view->rootContext()->setContextProperty("System",m_backend);
 view->setSource(QUrl("qrc:/qml/FilePicker.qml"));
 ++m_open;m_idle.stop();
 auto done=std::make_shared<bool>(false);
 const auto finish=[this,reply,view,session,done,appId,request,remember](bool accepted,const QStringList &paths,int filter){
  if(*done)return;*done=true;
  QVariantMap results;QStringList uris;
  if(accepted&&request.kind=="save-files"&&!paths.isEmpty()){
   for(const auto &name:request.saveNames)uris<<QString::fromUtf8(QUrl::fromLocalFile(QDir(paths.first()).absoluteFilePath(QFileInfo(name).fileName())).toEncoded());
  }else for(const auto &path:paths)uris<<QString::fromUtf8(QUrl::fromLocalFile(path).toEncoded());
  if(remember&&accepted&&!paths.isEmpty())rememberFolder(appId,request.kind=="folder"||request.kind=="save-files"?paths.first():QFileInfo(paths.first()).absolutePath());
  results.insert("uris",uris);
  if(request.kind=="open")results.insert("writable",true);
  Q_UNUSED(filter)
  QDBusConnection::sessionBus().send(reply.createReply(QVariantList{QVariant::fromValue(uint(accepted&&!uris.isEmpty()?0:1)),QVariant::fromValue(results)}));
  view->hide();view->deleteLater();session->deleteLater();
  if(--m_open<=0){m_open=0;m_idle.start();}
 };
 connect(session,&FilePickerSession::finished,this,finish);
 connect(view,&QQuickWindow::closing,session,[session]{session->cancel();});
 if(view->status()==QQuickView::Error){finish(false,{},-1);return;}
 view->show();view->raise();view->requestActivate();
}

static FilePickerSession::Request requestFrom(const QString &kind,const QString &title,const QVariantMap &options){
 FilePickerSession::Request request;
 request.kind=kind;request.title=title;
 request.acceptLabel=options.value("accept_label").toString().remove('_');
 request.multiple=options.value("multiple").toBool();
 request.filters=readFilters(options.value("filters"));
 request.currentFilterName=readFilterName(options.value("current_filter"));
 request.currentName=options.value("current_name").toString();
 return request;
}

uint FileChooserPortal::OpenFile(const QDBusObjectPath &,const QString &appId,const QString &,const QString &title,const QVariantMap &options,QVariantMap &){
 setDelayedReply(true);
 show(appId,requestFrom(options.value("directory").toBool()?"folder":"open",title,options),options);
 return 0;
}

uint FileChooserPortal::SaveFile(const QDBusObjectPath &,const QString &appId,const QString &,const QString &title,const QVariantMap &options,QVariantMap &){
 setDelayedReply(true);
 show(appId,requestFrom("save",title,options),options);
 return 0;
}

uint FileChooserPortal::SaveFiles(const QDBusObjectPath &,const QString &appId,const QString &,const QString &title,const QVariantMap &options,QVariantMap &){
 setDelayedReply(true);
 auto request=requestFrom("save-files",title,options);
 const auto files=options.value("files");
 if(files.metaType()==QMetaType::fromType<QByteArrayList>()){
  for(auto name:files.value<QByteArrayList>()){while(name.endsWith('\0'))name.chop(1);request.saveNames<<QString::fromUtf8(name);}
 }else if(files.canConvert<QDBusArgument>()){
  const auto argument=files.value<QDBusArgument>();
  argument.beginArray();
  while(!argument.atEnd()){QByteArray name;argument>>name;while(name.endsWith('\0'))name.chop(1);request.saveNames<<QString::fromUtf8(name);}
  argument.endArray();
 }
 show(appId,request,options);
 return 0;
}

int runFileChooserPortal(Backend *backend,SettingsService *settings){
 QGuiApplication::setQuitOnLastWindowClosed(false);
 FileChooserPortal portal(backend,settings);
 if(!portal.start()){std::fprintf(stderr,"Sovel: the file picker service is already running or the session bus is unavailable.\n");return 1;}
 return QGuiApplication::exec();
}

int previewFilePicker(Backend *backend,SettingsService *settings,const QString &kind,const QString &capture){
 FilePickerSession::Request request;
 request.kind=QStringList{"open","save","folder"}.contains(kind)?kind:"open";
 request.title=request.kind=="save"?"Save File":request.kind=="folder"?"Select Folder":"Open File";
 request.acceptLabel=request.kind=="open"?"Open":request.kind=="folder"?"Select Folder":"Save";
 request.multiple=request.kind=="open";
 request.currentName=request.kind=="save"?"Untitled.txt":QString();
 request.startFolder=request.kind=="save"?location(QStandardPaths::DownloadLocation):home();
 request.filters={QVariantMap{{"name","All files"},{"patterns",QStringList{"*"}}},QVariantMap{{"name","Images"},{"patterns",QStringList{"*.png","*.jpg","*.jpeg","*.webp"}}}};
 FilePickerSession session(request);
 QQuickView view;
 view.setTitle(request.title);view.setResizeMode(QQuickView::SizeRootObjectToView);view.resize(980,640);
 view.setColor(QColor(settings->colors().value("background").toString()));
 view.rootContext()->setContextProperty("Picker",&session);
 view.rootContext()->setContextProperty("SettingsService",settings);
 view.rootContext()->setContextProperty("System",backend);
 view.setSource(QUrl("qrc:/qml/FilePicker.qml"));
 if(view.status()==QQuickView::Error)return 2;
 QObject::connect(&session,&FilePickerSession::finished,qApp,[](bool accepted,const QStringList &paths,int){
  for(const auto &path:paths)std::printf("%s\n",qPrintable(path));
  QCoreApplication::exit(accepted?0:1);
 });
 view.show();
 if(!capture.isEmpty())QTimer::singleShot(1200,qApp,[&view,capture]{QCoreApplication::exit(view.grabWindow().save(capture)?0:3);});
 return QGuiApplication::exec();
}
