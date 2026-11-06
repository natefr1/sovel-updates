#include "displays.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QStandardPaths>
#include <QTimer>

namespace {
QJsonObject readObject(const QString &path)
{
 QFile file(path);
 if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024)return {};
 return QJsonDocument::fromJson(file.readAll()).object();
}
}

Displays::Displays(QObject *parent):QObject(parent)
{
 m_configDirectory=QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)+"/Sovel";
 const auto runtime=qEnvironmentVariable("XDG_RUNTIME_DIR");
 if(!runtime.isEmpty())m_runtimeDirectory=runtime+"/sovel-displays";
 QDir().mkpath(m_configDirectory);
 m_watcher.addPath(m_configDirectory);
 if(!m_runtimeDirectory.isEmpty()){
  QDir().mkpath(m_runtimeDirectory);
  m_watcher.addPath(m_runtimeDirectory);
 }
 connect(&m_watcher,&QFileSystemWatcher::directoryChanged,this,[this](const QString &path){
  if(path==m_configDirectory)loadConfiguration();
  if(path==m_runtimeDirectory){
   updateIdentification();
   if(pending()&&!QFile::exists(m_runtimeDirectory+"/pending.json")){
    m_token.clear();
    if(!busy())m_status="The display preview ended. Checking the current layout…";
    emit changed();detect();
   }
  }
 });
 connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
  if(error==QProcess::FailedToStart){m_status="Display controls could not start.";emit changed();}
 });
 connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
  const auto action=m_action;
  if(code!=0||exit!=QProcess::NormalExit){
   m_status=QString::fromUtf8(m_process.readAllStandardError()).trimmed().left(1500);
   if(m_status.isEmpty())m_status="The display service did not finish.";
   emit changed();
   return;
  }
  const auto result=QJsonDocument::fromJson(m_process.readAllStandardOutput()).object();
  if(result.contains("outputs"))m_outputs=result.value("outputs").toArray().toVariantList();
  if(result.contains("pending"))m_token=result.value("pending").toString();
  if(result.contains("message"))m_status=result.value("message").toString();
  if(action=="keep"||action=="revert")m_token.clear();
  emit changed();
  if(action=="keep"||action=="revert")detect();
 });
 connect(qApp,&QGuiApplication::primaryScreenChanged,this,[this]{emit changed();});
 connect(qApp,&QGuiApplication::screenAdded,this,[this]{emit changed();});
 connect(qApp,&QGuiApplication::screenRemoved,this,[this]{emit changed();});
 loadConfiguration();
 updateIdentification();
}

void Displays::loadConfiguration()
{
 const auto value=readObject(m_configDirectory+"/Displays.json").toVariantMap();
 if(value!=m_config){m_config=value;emit changed();}
}

void Displays::updateIdentification()
{
 if(m_runtimeDirectory.isEmpty())return;
 const auto data=readObject(m_runtimeDirectory+"/identify.json");
 const auto until=data.value("until").toDouble();
 const auto remaining=until*1000-QDateTime::currentMSecsSinceEpoch();
 const bool active=remaining>0&&remaining<=6500;
 const auto numbers=active?data.value("numbers").toObject().toVariantMap():QVariantMap();
 if(active!=m_identifying||numbers!=m_identification){m_identifying=active;m_identification=numbers;emit changed();}
 if(active)QTimer::singleShot(int(remaining)+50,this,&Displays::updateIdentification);
}

QString Displays::primary() const
{
 const auto saved=m_config.value("primary").toString();
 for(auto *screen:QGuiApplication::screens())if(screen->name()==saved)return saved;
 const auto *screen=QGuiApplication::primaryScreen();
 return screen?screen->name():QString();
}

void Displays::start(const QString &action,const QStringList &arguments,const QByteArray &input)
{
 if(busy())return;
 if(m_runtimeDirectory.isEmpty()){m_status="Display settings require a running Sovel session.";emit changed();return;}
 m_action=action;
 const QString source=QCoreApplication::applicationDirPath()+"/../packaging/sovel-displays";
 const QString program=QFileInfo::exists(source)?source:"/usr/libexec/sovel-displays";
 m_process.start("python3",QStringList{program,action}+arguments);
 if(!input.isEmpty())m_process.write(input);
 m_process.closeWriteChannel();
 emit changed();
}

void Displays::detect(){start("list");}
void Displays::identify(){start("identify");}
void Displays::preview(const QVariantMap &proposal)
{
 if(!pending())start("preview",{},QJsonDocument(QJsonObject::fromVariantMap(proposal)).toJson(QJsonDocument::Compact));
}
void Displays::keep(){if(pending())start("keep",{m_token});}
void Displays::revert(){if(pending())start("revert",{m_token});}
void Displays::restore(){if(!m_config.value("outputs").toList().isEmpty())start("restore");}
