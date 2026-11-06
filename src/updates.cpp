#include "updates.h"
#include <PackageKit/Daemon>
#include <QDateTime>
#include <QFile>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QProcess>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>

using Transaction=PackageKit::Transaction;
Updates::Updates(QObject *parent):QObject(parent),m_settings("Sovel","Desktop"){}
void Updates::refreshReleaseInfo(){
    if(m_releaseInfoBusy)return;m_releaseInfoBusy=true;auto p=new QProcess(this);auto output=std::make_shared<QByteArray>();auto finished=std::make_shared<bool>(false);auto timer=new QTimer(p);timer->setSingleShot(true);timer->start(20000);connect(timer,&QTimer::timeout,p,&QProcess::kill);
    auto done=[this,p,output,finished](bool ok){if(*finished)return;*finished=true;m_releaseInfoBusy=false;output->append(p->readAllStandardOutput());if(ok){const auto data=QJsonDocument::fromJson(*output).object();m_releaseDetails=data.value("installed").toObject().toVariantMap();auto releases=data.value("releases").toArray();for(auto package:m_packages)if(package.toMap().value("name")=="sovel-desktop")for(auto value:releases){auto release=value.toObject().toVariantMap();if(release.value("version")==package.toMap().value("version"))m_releaseDetails=release;}m_releaseHistory=data.value("history").toArray().toVariantList();for(auto &value:m_releaseHistory){auto record=value.toMap();record["downloadedAt"]=m_settings.value("updates/downloaded/"+record.value("version").toString());value=record;}m_releaseDetails["downloadedAt"]=m_settings.value("updates/downloaded/"+m_releaseDetails.value("version").toString());emit changed();}p->deleteLater();};
    connect(p,&QProcess::readyReadStandardOutput,this,[p,output]{output->append(p->readAllStandardOutput());if(output->size()>2*1024*1024)p->kill();});connect(p,&QProcess::errorOccurred,this,[done](QProcess::ProcessError e){if(e==QProcess::FailedToStart)done(false);});connect(p,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[done](int code,QProcess::ExitStatus status){done(code==0&&status==QProcess::NormalExit);});
    const QString helper=QFileInfo::exists("/usr/libexec/sovel-release-info")?"/usr/libexec/sovel-release-info":QCoreApplication::applicationDirPath()+"/../packaging/sovel-release-info";p->start("python3",{helper});
}
void Updates::setAutomaticChecks(bool enabled){
    if(automaticChecks()==enabled)return;
    m_settings.setValue("updates/automaticChecks",enabled);m_settings.sync();emit changed();
}
void Updates::startAutomaticChecks(){
    if(liveSession()||m_automaticTimer.isActive())return;
    m_automaticTimer.setInterval(60000);
    connect(&m_automaticTimer,&QTimer::timeout,this,&Updates::pollAutomaticChecks);
    m_automaticTimer.start();
    // Leave startup quiet. Only the long-lived desktop schedules checks.
    m_settings.sync();
    if(!m_settings.contains("updates/nextAutomaticCheck")){
        m_settings.setValue("updates/nextAutomaticCheck",QDateTime::currentDateTimeUtc().addSecs(120).toString(Qt::ISODate));m_settings.sync();
    }
}
void Updates::pollAutomaticChecks(){
    if(busy()||liveSession())return;
    m_settings.sync();if(!automaticChecks())return;
    auto now=QDateTime::currentDateTimeUtc();
    auto next=QDateTime::fromString(m_settings.value("updates/nextAutomaticCheck").toString(),Qt::ISODate);
    if(next.isValid()&&next>now)return;
    m_settings.setValue("updates/nextAutomaticCheck",now.addDays(1).toString(Qt::ISODate));m_settings.sync();
    m_background=true;check();
}
bool Updates::restartRequired() const{return m_restart||QFile::exists("/run/reboot-required")||QFile::exists("/run/sovel-desktop-restart-required");}
bool Updates::liveSession() const{return QFile::exists("/etc/sovel/live-image");}
void Updates::check(){
    if(busy())return;
    m_checked=false;m_error.clear();m_packages.clear();m_ids.clear();
    m_status="Downloading the latest update information…";
    attach(PackageKit::Daemon::refreshCache(true),Operation::Refresh);
}
void Updates::list(){
    m_status="Looking for available updates…";
    attach(PackageKit::Daemon::getUpdates(),Operation::List);
}
void Updates::install(){
    if(!canInstall())return;
    m_error.clear();m_checked=false;m_status="Installing verified updates. Keep your computer connected to power.";
    // PackageKit and APT enforce the repository signatures. Never accept a
    // signing key or retry an untrusted transaction automatically.
    attach(PackageKit::Daemon::updatePackages(m_ids,Transaction::TransactionFlagOnlyTrusted),Operation::Install);
}
void Updates::cancel(){if(canCancel())m_transaction->cancel();}
void Updates::attach(Transaction *transaction,Operation operation){
    m_operation=operation;
    m_transaction=transaction;
    if(!transaction){m_background=false;m_status="Updates could not be started.";m_error="The system update service is unavailable.";emit changed();return;}
    transaction->setHints(m_background?QStringList{"interactive=false","background=true"}:QStringList{"interactive=true","background=false"});
    connect(transaction,&Transaction::percentageChanged,this,&Updates::changed);
    connect(transaction,&Transaction::allowCancelChanged,this,&Updates::changed);
    connect(transaction,&Transaction::itemProgress,this,[this,operation](const QString &id,Transaction::Status status,uint percent){if(operation==Operation::Install&&status==Transaction::StatusDownload&&percent==100&&Transaction::packageName(id)=="sovel-desktop"){m_settings.setValue("updates/downloaded/"+Transaction::packageVersion(id),QDateTime::currentDateTimeUtc().toString(Qt::ISODate));m_settings.sync();}});
    connect(transaction,&Transaction::package,this,[this,operation](Transaction::Info info,const QString &id,const QString &summary){
        if(operation!=Operation::List||info==Transaction::InfoBlocked||info==Transaction::InfoUntrusted||m_ids.contains(id))return;
        m_ids<<id;m_packages<<QVariantMap{{"name",Transaction::packageName(id)},{"version",Transaction::packageVersion(id)},{"summary",summary},{"security",info==Transaction::InfoSecurity}};
        emit changed();
        refreshReleaseInfo();
    });
    connect(transaction,&Transaction::requireRestart,this,[this](Transaction::Restart type,const QString &){
        if(type==Transaction::RestartSystem||type==Transaction::RestartSecuritySystem){m_restart=true;emit changed();}
    });
    connect(transaction,&Transaction::errorCode,this,[this](Transaction::Error code,const QString &details){
        switch(code){
        case Transaction::ErrorGpgFailure:case Transaction::ErrorBadGpgSignature:case Transaction::ErrorMissingGpgSignature:
        case Transaction::ErrorCannotInstallRepoUnsigned:case Transaction::ErrorCannotUpdateRepoUnsigned:
            m_error="An update signature could not be verified. The update was stopped. No trust settings were changed.";break;
        case Transaction::ErrorNotAuthorized:m_error="Authorization was cancelled or denied. Try again to approve the update.";break;
        case Transaction::ErrorCannotGetLock:case Transaction::ErrorLockRequired:m_error="Another system update is running. Wait for it to finish, then try again.";break;
        case Transaction::ErrorNoNetwork:m_error="Connect to the internet, then check again.";break;
        case Transaction::ErrorTransactionCancelled:case Transaction::ErrorCancelledPriority:break;
        default:m_error=details.isEmpty()?"The update could not be completed. Try checking again.":details;break;
        }
        emit changed();
    });
    connect(transaction,&Transaction::finished,this,[this,transaction,operation](Transaction::Exit result,uint){
        if(m_transaction!=transaction)return;
        m_transaction=nullptr;
        if(result==Transaction::ExitCancelled||result==Transaction::ExitCancelledPriority){m_background=false;m_status="Update task cancelled.";m_checked=false;emit changed();return;}
        if(result!=Transaction::ExitSuccess||!m_error.isEmpty()){
            if(m_background){m_settings.setValue("updates/nextAutomaticCheck",QDateTime::currentDateTimeUtc().addSecs(3600).toString(Qt::ISODate));m_settings.sync();}
            m_background=false;
            m_status="Updates need attention.";m_checked=false;
            if(m_error.isEmpty())m_error="The update did not complete. Check your connection and try again.";
            emit changed();return;
        }
        if(operation==Operation::Refresh){list();return;}
        m_background=false;
        if(operation==Operation::Install){m_checked=true;m_packages.clear();m_ids.clear();m_status=restartRequired()?"Updates installed. Restart to finish.":"Updates installed successfully.";}
        else {
            m_checked=true;m_settings.setValue("updates/lastChecked",QDateTime::currentDateTime().toString(Qt::ISODate));m_settings.sync();
            m_status=m_ids.isEmpty()?"Sovel is up to date.":QString::number(m_ids.size())+" updates are available.";
        }
        refreshReleaseInfo();emit changed();
    });
    emit changed();
}
void Updates::restart(){
    if(busy())return;
    auto message=QDBusMessage::createMethodCall("org.freedesktop.login1","/org/freedesktop/login1","org.freedesktop.login1.Manager","Reboot");
    message<<true;
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        if(watcher->isError()){m_error="Restart could not be approved. Save your work and try again.";emit changed();}
        watcher->deleteLater();
    });
}
