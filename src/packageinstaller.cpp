#include "packageinstaller.h"
#include <PackageKit/Daemon>
#include <QtConcurrent>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QProcess>
#include <QDir>
using Tx=PackageKit::Transaction;
PackageInstaller::~PackageInstaller(){if(m_transaction)disconnect(m_transaction,nullptr,this,nullptr);}
void PackageInstaller::load(const QString &path){
 if(busy())return;m_error.clear();m_details.clear();m_changes.clear();m_reviewed=m_installed=false;
 const QFileInfo file(path);
 if(!file.isFile()||!file.isReadable()||file.suffix().compare("deb",Qt::CaseInsensitive)!=0){m_error="Choose a readable .deb package on this computer.";emit changed();return;}
 m_stage=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/sovel-package-XXXXXX");
 if(!m_stage->isValid()){m_error="There is not enough space to prepare this package.";emit changed();return;}
 m_loading=true;m_status="Reading package information…";emit changed();
 auto *watcher=new QFutureWatcher<QVariantMap>(this);const auto stage=m_stage;
 connect(watcher,&QFutureWatcher<QVariantMap>::finished,this,[this,watcher]{
  m_loading=false;const auto result=watcher->result();watcher->deleteLater();m_error=result.value("error").toString();
  if(m_error.isEmpty()){m_details=result;m_file=result.value("file").toString();m_hash=result.value("sha256").toString();m_status="Review this package before making system changes.";}
  else m_status="This package could not be prepared.";emit changed();
 });
 watcher->setFuture(QtConcurrent::run([stage,file]{
  QVariantMap result;const auto destination=stage->filePath("package.deb");
  if(!QFile::copy(file.absoluteFilePath(),destination))return QVariantMap{{"error","The package could not be copied. Check free space and permissions."}};
  QFile input(destination);if(!input.open(QIODevice::ReadOnly))return QVariantMap{{"error","The package could not be read."}};
  QCryptographicHash hash(QCryptographicHash::Sha256);if(!hash.addData(&input))return QVariantMap{{"error","The package could not be checked."}};
  QProcess process;process.start("dpkg-deb",{"--field",destination});
  if(!process.waitForFinished(20000)){process.kill();process.waitForFinished();return QVariantMap{{"error","Reading this package took too long."}};}
  if(process.exitCode()!=0||process.exitStatus()!=QProcess::NormalExit)return QVariantMap{{"error","This file is not a valid Debian package."}};
  const auto output=process.readAllStandardOutput();if(output.size()>256*1024)return QVariantMap{{"error","The package information is too large."}};
  QString field;for(const auto &line:QString::fromUtf8(output).split('\n')){
   if(line.startsWith(' ')&&!field.isEmpty()){result[field]=result.value(field).toString()+"\n"+(line.mid(1)=="."?QString():line.mid(1));continue;}
   const auto colon=line.indexOf(':');if(colon<1){field.clear();continue;}field=line.left(colon);result[field]=line.mid(colon+1).trimmed();
  }
  if(result.value("Package").toString().isEmpty()||result.value("Version").toString().isEmpty())return QVariantMap{{"error","This package is missing required information."}};
  const auto arch=result.value("Architecture").toString();if(arch!="amd64"&&arch!="all")return QVariantMap{{"error","This package is built for "+arch+", rather than this computer's amd64 architecture."}};
  result["sha256"]=QString::fromLatin1(hash.result().toHex());result["file"]=destination;result["filename"]=file.fileName();return result;
 }));
}
void PackageInstaller::review(){
 if(busy()||m_file.isEmpty()||m_installed)return;
 m_error.clear();m_changes.clear();m_reviewed=false;m_status="Checking dependencies and installation changes…";
 attach(PackageKit::Daemon::installFiles({m_file},Tx::TransactionFlagSimulate),true);
}
void PackageInstaller::install(){
 if(busy()||!m_reviewed||m_file.isEmpty()||m_installed)return;
 m_error.clear();m_reviewed=false;m_status="Installing package. Approve the system authorization prompt.";
 // Local downloads are explicitly reviewed by the user. Repository update
 // trust policy is unchanged; no signing keys are accepted automatically.
 attach(PackageKit::Daemon::installFiles({m_file},Tx::TransactionFlagNone),false);
}
void PackageInstaller::cancel(){if(canCancel())m_transaction->cancel();}
void PackageInstaller::attach(Tx *transaction,bool simulate){
 m_transaction=transaction;
 if(!transaction){m_error="The system package service is unavailable.";emit changed();return;}
 transaction->setHints({"interactive=true","background=false"});
 connect(transaction,&Tx::percentageChanged,this,&PackageInstaller::changed);connect(transaction,&Tx::allowCancelChanged,this,&PackageInstaller::changed);
 connect(transaction,&Tx::package,this,[this,simulate](Tx::Info info,const QString &id,const QString &summary){
  if(!simulate)return;const auto name=Tx::packageName(id);QString action;
  if(info==Tx::InfoRemoving)action="Remove";else if(info==Tx::InfoUpdating)action="Update";else if(info==Tx::InfoInstalling)action="Install";else return;
  const QStringList protectedPackages{"sovel-desktop","greetd","labwc","libc6","systemd","network-manager","linux-generic"};
  if(action=="Remove"&&(protectedPackages.contains(name)||name.startsWith("linux-image")))m_error="This package would remove an essential system component. Installation has been blocked.";
  m_changes<<QVariantMap{{"action",action},{"name",name},{"version",Tx::packageVersion(id)},{"summary",summary}};emit changed();
 });
 connect(transaction,&Tx::errorCode,this,[this](Tx::Error code,const QString &text){
  m_error=code==Tx::ErrorNotAuthorized?"Authorization was cancelled or denied.":text.isEmpty()?"The package could not be installed.":text;emit changed();
 });
 connect(transaction,&Tx::finished,this,[this,transaction,simulate](Tx::Exit exit,uint){
  if(m_transaction!=transaction)return;m_transaction=nullptr;
  if(exit==Tx::ExitSuccess&&m_error.isEmpty()){m_reviewed=simulate;m_installed=!simulate;m_status=simulate?"Review the changes below, then choose Install.":"Installation complete. Find the app in App Library.";}
  else {m_reviewed=false;m_status=exit==Tx::ExitCancelled?"Installation cancelled.":"Installation needs attention.";if(m_error.isEmpty()&&exit!=Tx::ExitCancelled)m_error="The package service could not complete this request.";}
  emit changed();
 });
 emit changed();
}
