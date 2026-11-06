#include "audio.h"
#include <QtTest>
#include <QProcess>
#include <QFile>
#include <QtEndian>
#include <cmath>
#include <cstring>
class AudioLiveTests:public QObject {
 Q_OBJECT
 Audio audio;
 QString hdmi="sovel_hdmi_07",usb="sovel_usb_07",mic="sovel_mic_07";
 QProcess player;
 QTimer feed;
 QByteArray tone;
 QVariantMap output(const QString &name){for(auto v:audio.outputs())if(v.toMap()["name"]==name)return v.toMap();return {};}
 QVariantMap stream(){for(auto v:audio.streams())if(v.toMap()["description"].toString().contains("SovelAudioTest"))return v.toMap();return {};}
 QByteArray run(const QStringList &args){QProcess p;p.start("pactl",args);if(!p.waitForFinished(10000)||p.exitCode()!=0){qWarning()<<p.readAllStandardError();return {};}return p.readAllStandardOutput();}
private slots:
 void initTestCase(){
  QVERIFY(!run({"load-module","module-null-sink","sink_name="+hdmi,"sink_properties=device.description=Monitor-HDMI"}).isEmpty());
  QVERIFY(!run({"load-module","module-null-sink","sink_name="+usb,"sink_properties=device.description=USB-Speakers"}).isEmpty());
  QVERIFY(!run({"load-module","module-remap-source","master="+hdmi+".monitor","source_name="+mic,"source_properties=device.description=Test-Microphone"}).isEmpty());
  audio.setWatching(true);QTRY_VERIFY_WITH_TIMEOUT(!output(hdmi).isEmpty(),15000);QTRY_VERIFY_WITH_TIMEOUT(!output(usb).isEmpty(),15000);
 }
 void selectMonitorVolumeBalanceAndMute(){
  QCOMPARE(output(hdmi)["kind"].toString(),QString("Monitor · HDMI"));audio.selectOutput(hdmi);QTRY_COMPARE_WITH_TIMEOUT(audio.defaultOutput(),hdmi,15000);
  audio.volume(hdmi,35);QTRY_VERIFY_WITH_TIMEOUT(qAbs(output(hdmi)["volume"].toInt()-35)<=1,15000);
  audio.balance(hdmi,0.5);QTRY_VERIFY_WITH_TIMEOUT(output(hdmi)["balance"].toDouble()>0.45,15000);
  audio.volume(hdmi,60);QTRY_VERIFY_WITH_TIMEOUT(qAbs(output(hdmi)["volume"].toInt()-60)<=1,15000);QVERIFY(output(hdmi)["balance"].toDouble()>0.4);
  audio.balance(hdmi,0);audio.mute(hdmi,true);QTRY_VERIFY_WITH_TIMEOUT(output(hdmi)["muted"].toBool(),15000);audio.mute(hdmi,false);QTRY_VERIFY_WITH_TIMEOUT(!output(hdmi)["muted"].toBool(),15000);
 }
 void routeAndControlARealApplicationStream(){
  player.setStandardInputFile("/dev/zero");
  player.start("pacat",{"--playback","--raw","--format=s16le","--rate=48000","--channels=2","--client-name=SovelAudioTest","--device="+hdmi});QVERIFY(player.waitForStarted(3000));
  QTRY_VERIFY_WITH_TIMEOUT(!stream().isEmpty(),15000);const int id=stream()["id"].toInt();audio.streamVolume(id,22);QTRY_VERIFY_WITH_TIMEOUT(qAbs(stream()["volume"].toInt()-22)<=1,15000);
  audio.refresh();QCOMPARE(stream()["id"].toInt(),id);audio.moveStream(id,usb);QTRY_COMPARE_WITH_TIMEOUT(stream()["targetId"].toInt(),output(usb)["id"].toInt(),15000);
  for(const auto &destination:{hdmi,usb,hdmi,usb}){audio.moveStream(stream()["id"].toInt(),destination);QTRY_COMPARE_WITH_TIMEOUT(stream()["targetId"].toInt(),output(destination)["id"].toInt(),15000);QTest::qWait(600);QCOMPARE(stream()["targetId"].toInt(),output(destination)["id"].toInt());}
  audio.streamMute(id,true);QTRY_VERIFY_WITH_TIMEOUT(stream()["muted"].toBool(),15000);
 }
 void realMicrophoneMeterAndRecording(){
  audio.selectInput(mic);QTRY_COMPARE_WITH_TIMEOUT(audio.defaultInput(),mic,15000);audio.monitorInput(mic,true);
  audio.testSpeaker(hdmi,0);QTRY_VERIFY_WITH_TIMEOUT(audio.inputLevel()>0.003,5000);
  audio.recordTest(mic);QTRY_VERIFY_WITH_TIMEOUT(audio.recording(),3000);audio.testSpeaker(hdmi,1);QTRY_VERIFY_WITH_TIMEOUT(!audio.recording(),9000);
  QFile recording(audio.lastRecording());QVERIFY(recording.open(QIODevice::ReadOnly));const auto data=recording.readAll();QVERIFY(data.size()>1000);QVERIFY(data.startsWith("RIFF")||data.startsWith("RF64"));QVERIFY(data.left(64).contains("WAVE"));
 }
 void leftAndRightTestsContainDistinctChannels(){
  for(int channel=0;channel<2;channel++){
   QProcess capture;capture.start("parec",{"--client-name=SovelChannelCapture","--device="+hdmi+".monitor","--format=float32le","--rate=48000","--channels=2","--latency-msec=30","--raw"});QVERIFY(capture.waitForStarted(3000));
   auto connected=[this]{for(auto value:audio.streams())if(value.toMap()["description"].toString()=="SovelChannelCapture")return true;return false;};QTRY_VERIFY_WITH_TIMEOUT(connected(),10000);
   QTest::qWait(300);capture.readAllStandardOutput();audio.testSpeaker(hdmi,channel);QTest::qWait(2500);capture.kill();capture.waitForFinished(1000);auto bytes=capture.readAllStandardOutput();double energy[2]={0,0};
   for(qsizetype i=0;i+7<bytes.size();i+=8)for(int c=0;c<2;c++){auto bits=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()+i+c*4));float value;std::memcpy(&value,&bits,4);if(std::isfinite(value))energy[c]+=value*value;}
   qInfo()<<"Captured channel"<<channel<<"bytes"<<bytes.size()<<"energies"<<energy[0]<<energy[1]<<capture.readAllStandardError();QVERIFY(energy[channel]>0.01);QVERIFY(energy[channel]>energy[1-channel]*50);
  }
 }
 void subscriptionDoesNotSelfPollFromItsOwnClients(){QSignalSpy updates(&audio,&Audio::changed);QTest::qWait(2500);QVERIFY2(updates.size()<20,"Audio refresh is looping on its own pactl client events");}
 void cleanupTestCase(){feed.stop();player.terminate();if(!player.waitForFinished(1000))player.kill();audio.setWatching(false);}
};
QTEST_GUILESS_MAIN(AudioLiveTests)
#include "audio-live.moc"
