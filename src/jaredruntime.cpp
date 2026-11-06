#include "jared.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QtEndian>

namespace {
const QString OllamaHost="http://127.0.0.1:11434";
const QString Model="qwen3.5:4b";
QNetworkRequest localRequest(const QString &path)
{
 QNetworkRequest request(QUrl(OllamaHost+path));
 request.setTransferTimeout(4500);
 request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
 request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
 return request;
}
QString piperExecutable()
{
 const auto installed=QDir::home().filePath(".local/share/Sovel/voice-venv/bin/piper");
 if(QFileInfo(installed).isExecutable())return installed;
 return QStandardPaths::findExecutable("piper-tts");
}
bool writeWave(const QString &raw,const QString &output)
{
 QFile source(raw);
 if(!source.open(QIODevice::ReadOnly)||source.size()<3200||source.size()>2*1024*1024)return false;
 QFile target(output);
 if(!target.open(QIODevice::WriteOnly|QIODevice::Truncate))return false;
 const auto size=quint32(source.size());
 QByteArray header("RIFF",4);
 auto append32=[&](quint32 value){char bytes[4];qToLittleEndian(value,bytes);header.append(bytes,4);};
 auto append16=[&](quint16 value){char bytes[2];qToLittleEndian(value,bytes);header.append(bytes,2);};
 append32(36+size);
 header.append("WAVEfmt ",8);append32(16);append16(1);append16(1);append32(16000);
 append32(32000);append16(2);append16(16);header.append("data",4);append32(size);
 return target.write(header)==44&&target.write(source.readAll())==size;
}
}

void Jared::setModelPreference(const QString &preference)
{
 if(m_busy||!QStringList{"auto","light"}.contains(preference)||preference==modelPreference())return;
 unload();
 m_settings.setValue("jared/modelPreference",preference);
 m_settings.sync();
 emit changed();
}

void Jared::chooseRuntime()
{
 const auto utility=QStandardPaths::findExecutable("nvidia-smi");
 if(utility.isEmpty()){startLightweightModel();return;}
 m_busy=true;
 m_status="Checking available GPU memory…";
 emit changed();
 const auto generation=m_requestGeneration;
 auto *probe=new QProcess(this);
 connect(probe,&QProcess::errorOccurred,this,[this,probe,generation](QProcess::ProcessError error){
  if(error!=QProcess::FailedToStart)return;
  probe->deleteLater();
  if(generation==m_requestGeneration&&!m_prompt.isEmpty()){m_busy=false;startLightweightModel();}
 });
 connect(probe,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,probe,generation](int code,QProcess::ExitStatus exit){
  const auto output=QString::fromUtf8(probe->readAllStandardOutput());
  probe->deleteLater();
  if(generation!=m_requestGeneration||m_prompt.isEmpty())return;
  bool enough=false;
  if(code==0&&exit==QProcess::NormalExit){
   for(const auto &line:output.split('\n')){
    const auto fields=line.split(',');
    if(fields.size()!=3)continue;
    const auto capability=fields[0].trimmed().toDouble();
    const auto driver=fields[1].trimmed().section('.',0,0).toInt();
    const auto memory=fields[2].trimmed().toInt();
    if(capability>=5&&driver>=(capability<=6.2?570:550)&&memory>=5200){enough=true;break;}
   }
  }
  if(!enough){m_busy=false;startLightweightModel();return;}
  auto *reply=m_network.get(localRequest("/api/tags"));
  connect(reply,&QNetworkReply::finished,this,[this,reply,generation]{
   const auto data=QJsonDocument::fromJson(reply->readAll()).object();
   bool present=false;
   if(reply->error()==QNetworkReply::NoError)for(const auto &entry:data.value("models").toArray())
    if(entry.toObject().value("name").toString()==Model){present=true;break;}
   reply->deleteLater();
   if(generation!=m_requestGeneration||m_prompt.isEmpty())return;
   m_busy=false;
   if(!present){startLightweightModel();return;}
   m_ollamaHost=OllamaHost;
   m_useOllama=true;
   m_runtimeLabel="Qwen3.5 4B · local";
   completeChat();
  });
 });
 QTimer::singleShot(3500,probe,[probe]{if(probe->state()!=QProcess::NotRunning)probe->kill();});
 probe->start(utility,{"--query-gpu=compute_cap,driver_version,memory.free","--format=csv,noheader,nounits"});
}

void Jared::verifyGpuRuntime()
{
 auto *reply=m_network.get(localRequest("/api/ps"));
 connect(reply,&QNetworkReply::finished,this,[this,reply]{
  const auto models=QJsonDocument::fromJson(reply->readAll()).object().value("models").toArray();
  reply->deleteLater();
  if(!m_useOllama)return;
  for(const auto &entry:models){
   const auto model=entry.toObject();
   if(model.value("name").toString()!=Model)continue;
   const auto vram=model.value("size_vram").toDouble();
   m_runtimeLabel=vram>0?QString("Qwen3.5 4B · GPU · %1 GiB VRAM").arg(vram/1073741824,0,'f',1)
                             :QString("Qwen3.5 4B · CPU");
   emit changed();
   return;
  }
 });
}

void Jared::downloadGpuModel()
{
 if(m_busy||m_gpuDownload.state()!=QProcess::NotRunning)return;
 const auto executable=QStandardPaths::findExecutable("ollama");
 if(executable.isEmpty()){m_status="Install the local Ollama runtime first, then download Qwen3.5 4B here.";emit changed();return;}
 auto environment=QProcessEnvironment::systemEnvironment();
 environment.insert("OLLAMA_HOST",OllamaHost);
 m_gpuDownload.setProcessEnvironment(environment);
 m_busy=true;
 m_status="Downloading Qwen3.5 4B to local Ollama storage…";
 emit changed();
 m_gpuDownload.start(executable,{"pull",Model});
}

void Jared::initializeVoice()
{
 m_recordLimit.setSingleShot(true);
 m_recordLimit.setInterval(60000);
 connect(&m_recordLimit,&QTimer::timeout,this,&Jared::stopVoice);
 m_transcribeLimit.setSingleShot(true);
 m_transcribeLimit.setInterval(120000);
 connect(&m_transcribeLimit,&QTimer::timeout,&m_transcriber,&QProcess::kill);
 connect(&m_recorder,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
  if(error==QProcess::FailedToStart){m_recording=false;m_recordLimit.stop();m_status="Microphone capture could not start.";emit changed();}
 });
 connect(&m_recorder,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int,QProcess::ExitStatus){
  m_recording=false;m_recordLimit.stop();
  const auto raw=m_runtime.filePath("microphone.raw");
  const auto wave=m_runtime.filePath("microphone.wav");
  if(!m_recordAccepted||!writeWave(raw,wave)){
   QFile::remove(raw);
   m_status="No usable microphone recording was captured.";emit changed();return;
  }
  QFile::remove(raw);
  const auto model=whisperModel();
  const auto binary=QStandardPaths::findExecutable("whisper-cli");
  m_busy=true;m_status="Transcribing on the CPU…";emit changed();
  QFile::remove(m_runtime.filePath("transcript.txt"));
  m_transcriber.start(binary,{"--no-gpu","--model",model,"--file",wave,"--language","en",
    "--threads",QString::number(qBound(1,QThread::idealThreadCount()/2,4)),"--no-timestamps",
    "--output-txt","--output-file",m_runtime.filePath("transcript")});
  m_transcribeLimit.start();
 });
 connect(&m_transcriber,&QProcess::readyReadStandardOutput,this,[this]{m_transcriber.readAllStandardOutput();});
 connect(&m_transcriber,&QProcess::readyReadStandardError,this,[this]{m_transcriber.readAllStandardError();});
 connect(&m_transcriber,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
  if(error!=QProcess::FailedToStart)return;
  m_transcribeLimit.stop();m_busy=false;m_status="Whisper could not start.";QFile::remove(m_runtime.filePath("microphone.wav"));emit changed();
 });
 connect(&m_transcriber,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
  m_transcribeLimit.stop();m_busy=false;
  const auto path=m_runtime.filePath("transcript.txt");
  QFile transcript(path);
  if(code==0&&exit==QProcess::NormalExit&&transcript.open(QIODevice::ReadOnly))
   m_transcript=QString::fromUtf8(transcript.read(3000)).trimmed();
  transcript.close();
  QFile::remove(path);QFile::remove(m_runtime.filePath("microphone.wav"));
  m_status=m_transcript.isEmpty()?"No speech was recognized.":"Review the transcription before sending.";
  emit changed();
 });
 connect(&m_voice,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
  const auto wave=m_runtime.filePath("reply.wav");
  if(code==0&&exit==QProcess::NormalExit&&QFileInfo(wave).size()>44)m_speech.start("paplay",{wave});
  else {m_status="The selected Piper voice could not speak.";emit changed();}
 });
 connect(&m_voice,&QProcess::readyReadStandardError,this,[this]{m_voice.readAllStandardError();});
 connect(&m_gpuDownload,&QProcess::readyReadStandardOutput,this,[this]{m_gpuDownload.readAllStandardOutput();});
 connect(&m_gpuDownload,&QProcess::readyReadStandardError,this,[this]{m_gpuDownload.readAllStandardError();});
 connect(&m_gpuDownload,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
  if(error==QProcess::FailedToStart){m_busy=false;m_status="Ollama could not start.";emit changed();}
 });
 connect(&m_gpuDownload,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
  m_busy=false;m_status=code==0&&exit==QProcess::NormalExit?
   "Qwen3.5 4B is available to Ollama. JARED checks the GPU before loading.":"The Ollama download did not finish.";
  emit changed();
 });
}

void Jared::setVoiceFile(const QString &kind,const QUrl &file)
{
 if(m_busy||m_recording||!file.isLocalFile())return;
 const QFileInfo info(file.toLocalFile());
 if(!info.isFile()||!info.isReadable())return;
 if(kind=="whisper"&&info.suffix().toLower()=="bin")m_settings.setValue("jared/whisperModel",info.canonicalFilePath());
 else if(kind=="piper"&&info.suffix().toLower()=="onnx"&&QFileInfo::exists(info.absoluteFilePath()+".json"))
  m_settings.setValue("jared/piperVoice",info.canonicalFilePath());
 else {m_status="Choose a Whisper .bin model or a Piper .onnx voice with its matching .onnx.json file.";emit changed();return;}
 m_settings.sync();m_status="Voice selection saved.";emit changed();
}

void Jared::recordVoice()
{
 if(m_recording||m_busy||!m_pending.isEmpty()||!m_runtime.isValid())return;
 if(!QFileInfo(whisperModel()).isReadable()||QStandardPaths::findExecutable("whisper-cli").isEmpty()){
  m_status="Choose a Whisper speech model in Preferences before recording.";emit changed();return;
 }
 m_transcript.clear();m_recordAccepted=false;m_recording=true;
 m_recorder.setStandardOutputFile(m_runtime.filePath("microphone.raw"));
 m_recorder.start("parec",{"--raw","--rate=16000","--channels=1","--format=s16le"});
 m_status="Recording your microphone…";m_recordLimit.start();emit changed();
}

void Jared::stopVoice()
{
 if(!m_recording)return;
 m_recordAccepted=true;
 m_recorder.terminate();
 QTimer::singleShot(1000,&m_recorder,[this]{if(m_recorder.state()!=QProcess::NotRunning)m_recorder.kill();});
}

void Jared::clearTranscript(){m_transcript.clear();emit changed();}

void Jared::speak(const QString &text)
{
 if(text.trimmed().isEmpty()||m_speech.state()!=QProcess::NotRunning||m_voice.state()!=QProcess::NotRunning)return;
 const auto managed=m_settings.value("jared/piperRuntime").toString();
 const bool hasManagedRuntime=QFileInfo(managed).isExecutable();
 const auto binary=hasManagedRuntime?managed:piperExecutable();
 const auto voice=piperVoice();
 if(!binary.isEmpty()&&QFileInfo(voice).isReadable()&&m_runtime.isValid()){
  const auto wave=m_runtime.filePath("reply.wav");
  QFile::remove(wave);
  QStringList arguments;
  if(hasManagedRuntime)arguments<<"-m"<<"piper";
  arguments<<"--model"<<voice<<"--output-file"<<wave;
  m_voice.start(binary,arguments);
  m_voice.write(text.left(8000).toUtf8()+"\n");
  m_voice.closeWriteChannel();
  return;
 }
 if(QStandardPaths::findExecutable("espeak-ng").isEmpty())return;
 m_speech.start("espeak-ng",{"--stdin"});
 m_speech.write(text.left(8000).toUtf8());
 m_speech.closeWriteChannel();
}
