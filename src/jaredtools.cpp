#include "jared.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace {
QStringList searchLocations()
{
 return {QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
         QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
         QDir::home().filePath("Projects")};
}
}

bool Jared::workIntent(const QString &original)
{
 QString text=original;
 text.remove(QRegularExpression("^jared[, :]\\s*",QRegularExpression::CaseInsensitiveOption));
 const auto match=[&](const QString &pattern){return QRegularExpression(pattern,QRegularExpression::CaseInsensitiveOption).match(text);};
 const auto note=match("^(?:take|make|save|create) (?:a )?note(?: that| saying|:)?\\s+(.+)$");
 if(note.hasMatch()){
  const auto body=note.captured(1).trimmed();
  propose({{"type","workspace"},{"operation","note-save"},{"sensitive",true},
           {"label","Save this note in JARED: "+body},
           {"arguments",QVariantMap{{"title",body.left(72)},{"body",body}}}});
  return true;
 }
 const auto find=match("^(?:find|locate|search (?:files )?for) (.+)$");
 if(find.hasMatch()){work("find",{{"query",find.captured(1)}});return true;}
 const auto web=match("^(?:search (?:the )?web for|look up online) (.+)$");
 if(web.hasMatch()){work("web-search",{{"query",web.captured(1)}});return true;}
 if(match("^open (?:my )?downloads[.!]?$").hasMatch()){
  propose({{"type","downloads"},{"label","Open Downloads in Sovel Files"}});
  return true;
 }
 const auto project=match("^open (?:my |the )?(.+?) project[.!]?$");
 if(project.hasMatch()){work("project-open",{{"name",project.captured(1)}});return true;}
 const auto wifi=match("^connect to (?:saved (?:wi-fi|wifi)(?: network)? )(.+)$");
 if(wifi.hasMatch()){
  const auto name=wifi.captured(1).trimmed();
  propose({{"type","workspace"},{"operation","wifi-connect"},{"sensitive",true},
           {"label","Connect to saved Wi-Fi profile "+name},{"arguments",QVariantMap{{"name",name}}}});
  return true;
 }
 const auto appearance=match("^(?:use|switch to|enable) (light|automatic) (?:mode|theme|appearance)[.!]?$");
 if(appearance.hasMatch()){
  const auto mode=appearance.captured(1).toLower()=="light"?"light":"auto";
  propose({{"type","appearance"},{"mode",mode},{"sensitive",true},{"label","Switch to "+appearance.captured(1)+" appearance"}});
  return true;
 }
 if(match("^(?:show |check )?(?:my )?gpu (?:usage|memory)[?!.]*$").hasMatch()){
  work("gpu",{});return true;
 }
 const auto folder=match(R"rx(^create (?:a )?folder "([^"]+)" in "([^"]+)"$)rx");
 if(folder.hasMatch()){
  const auto name=folder.captured(1);
  const auto parent=folder.captured(2);
  propose({{"type","workspace"},{"operation","folder-create"},{"sensitive",true},
           {"label","Create "+name+" in "+parent},{"arguments",QVariantMap{{"path",parent},{"name",name}}}});
  return true;
 }
 const auto move=match(R"rx(^move (?:the )?(?:file )?"([^"]+)" to "([^"]+)"$)rx");
 if(move.hasMatch()){
  propose({{"type","workspace"},{"operation","file-move"},{"sensitive",true},
           {"label","Move "+move.captured(1)+" to "+move.captured(2)},
           {"arguments",QVariantMap{{"path",move.captured(1)},{"destination",move.captured(2)}}}});
  return true;
 }
 return false;
}

void Jared::work(const QString &operation,const QVariantMap &arguments)
{
 static const QStringList allowed{"list","note-save","note-read","project-add","project-open","project-summary","task-add","task-extract","task-add-list","task-toggle","task-set",
    "research-save","research-read","web-search","article","find","open","open-preview","voice-download","piper-install","gpu","folder-create","file-move","wifi-connect"};
 if(!allowed.contains(operation)||m_workProcess||m_busy||!m_pending.isEmpty())return;
 auto *process=new QProcess(this);
 m_workProcess=process;
 const QString source=QCoreApplication::applicationDirPath()+"/../packaging/sovel-jared-workspace";
 const QString helper=QFileInfo::exists(source)?source:"/usr/libexec/sovel-jared-workspace";
 auto request=arguments;
 request["roots"]=searchLocations();
 const auto input=QJsonDocument(QJsonObject::fromVariantMap(request)).toJson(QJsonDocument::Compact);
 if(input.size()>1024*1024){m_workProcess=nullptr;process->deleteLater();return;}
 m_busy=true;
 if(operation=="note-save"||operation=="note-read")m_workRecord.clear();
 m_status=operation=="article"?"Retrieving public article text…":operation=="voice-download"?
          "Downloading the verified English speech model…":operation=="piper-install"?
          "Installing the verified CPU voice and Piper runtime…":"Working in your local workspace…";
 emit changed();
 connect(process,&QProcess::started,this,[process,input]{process->write(input);process->closeWriteChannel();});
 connect(process,&QProcess::readyReadStandardOutput,this,[process]{if(process->bytesAvailable()>512*1024)process->kill();});
 connect(process,&QProcess::errorOccurred,this,[this,process](QProcess::ProcessError error){
  if(error!=QProcess::FailedToStart)return;
  m_workProcess=nullptr;
  process->deleteLater();
  actionFeedback(false);
  answer("The JARED workspace service could not start.");
 });
 connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,operation](int code,QProcess::ExitStatus exit){
  if(m_workProcess!=process){process->deleteLater();return;}
  m_workProcess=nullptr;
  m_busy=false;
  const auto result=QJsonDocument::fromJson(process->readAllStandardOutput());
  if(code==0&&exit==QProcess::NormalExit&&result.isObject()){
   actionFeedback(true);
   workFinished(operation,result.object());
  }
  else {
   actionFeedback(false);
   const auto error=QString::fromUtf8(process->readAllStandardError()).trimmed().left(700);
   answer(error.isEmpty()?"The workspace action did not finish.":error);
  }
  process->deleteLater();
 });
 QTimer::singleShot(operation=="piper-install"?600000:operation=="voice-download"?150000:25000,process,[process]{if(process->state()!=QProcess::NotRunning)process->kill();});
 process->start("python3",{helper,operation});
}
QStringList Jared::availableDisplays() const
{
 QStringList names;
 for(auto *screen:QGuiApplication::screens())if(!screen->name().isEmpty())names.append(screen->name());
 return names;
}

void Jared::workFinished(const QString &operation,const QJsonObject &result)
{
 if(result.contains("workspace"))m_workspaces=result.value("workspace").toObject().toVariantMap();
 if(result.contains("record"))m_workRecord=result.value("record").toObject().toVariantMap();
 if(operation=="voice-download"){
  setVoiceFile("whisper",QUrl::fromLocalFile(result.value("voiceFile").toString()));
  return;
 }
 if(operation=="piper-install"){
  m_settings.setValue("jared/piperRuntime",result.value("runtime").toString());
  setVoiceFile("piper",QUrl::fromLocalFile(result.value("voiceFile").toString()));
  return;
 }
 if(operation=="project-summary"){
  if(m_projectRecent.size()>=24)m_projectRecent.clear();
  m_projectRecent[QString::number(result.value("project").toInt())]=result.value("files").toArray().toVariantList();
  m_status="Project files refreshed.";
  emit changed();
  return;
 }
 if(operation=="find"){
  m_fileMatches=result.value("matches").toArray().toVariantList();
  if(m_fileMatches.isEmpty()){answer("I found no matching items in Documents, Downloads or your projects.");return;}
  QStringList names;
  for(const auto &item:m_fileMatches.mid(0,5))names<<item.toMap().value("name").toString();
  answer(QString("I found %1 matching items. Open the Find tab to choose one.\n%2")
         .arg(m_fileMatches.size()).arg(names.join("\n")));
  return;
 }
 if(operation=="task-extract"){
  const auto tasks=result.value("tasks").toArray();
  if(tasks.isEmpty()){answer("I found no new unchecked tasks or TODO lines in that note.");return;}
  QStringList labels;
  for(const auto &task:tasks)labels<<task.toString();
  propose({{"type","workspace"},{"operation","task-add-list"},{"sensitive",true},
           {"label","Add these tasks to "+result.value("projectName").toString()+"?\n• "+labels.join("\n• ")},
           {"arguments",QVariantMap{{"project",result.value("project").toInt()},{"tasks",tasks.toVariantList()}}}});
  return;
 }
 if(operation=="open"||(operation=="project-open"&&result.contains("path"))){
  const auto path=result.value("path").toString();
  const bool opened=QFileInfo(path).isDir()
   ?QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--open-folder",path})
   :QDesktopServices::openUrl(QUrl::fromLocalFile(path));
  if(operation=="project-open"){answer(opened?"Opened "+result.value("projectName").toString()+" in Files.":"Files could not open that project.");return;}
  m_status=opened?"Opened the requested item.":"The item could not be opened.";
 }else if(operation=="open-preview"){
  const auto display=result.value("display").toString();
  const auto path=result.value("path").toString();
  if(!availableDisplays().contains(display)){answer("That display is no longer connected.");return;}
  const bool started=QProcess::startDetached(QCoreApplication::applicationFilePath(),{"--preview-file",path,"--display-output",display});
  m_status=started?"Opening the file preview on "+display+".":"Sovel Files could not open the preview.";
 }else if(operation=="web-search"){
  QDesktopServices::openUrl(QUrl(result.value("url").toString()));
  m_status="Opened web results in your browser.";
 }else if(operation=="article"){
  m_articleText=result.value("body").toString();
  m_status="Article text is ready in Research.";
 }else{
  const auto message=result.value("message").toString();
  if(!message.isEmpty()&&QStringList{"note-save","project-add","task-add","task-add-list","task-toggle","task-set","research-save","folder-create","file-move","wifi-connect"}.contains(operation)){
   m_status=message;
   add("action",message);
   return;
  }
  if(!message.isEmpty()){answer(message);return;}
  m_status="Workspace ready.";
 }
 emit changed();
}

void Jared::summarize(const QString &text,const QString &source)
{
 if(m_busy||!m_pending.isEmpty()||text.trimmed().isEmpty())return;
 if(!cachedVerification()){answer("Install and verify the local conversation model before summarizing.");return;}
 add("user","Summarize "+source.left(160));
 m_readOnlyContext=text.left(4800);
 beginConversation("Summarize the supplied document.");
}

