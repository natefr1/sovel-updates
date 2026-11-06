#include "audio.h"
#include <QtTest>
#include <QTemporaryDir>
class AudioTests:public QObject {
 Q_OBJECT
 QTemporaryDir config;
private slots:
 void initTestCase(){qputenv("XDG_CONFIG_HOME",config.path().toUtf8());}
 void realPulseFormatPreservesMonitorIdentityAndChannels(){
  QJsonObject o{{"index",17},{"name","alsa_output.pci.hdmi-stereo"},{"description","Monitor HDMI"},{"mute",false},{"sample_specification","s32le 2ch 48000Hz"},{"channel_map","front-left,front-right"},{"volume",QJsonObject{{"front-left",QJsonObject{{"value",32768}}},{"front-right",QJsonObject{{"value",32768}}}}}};
  auto result=Audio::device(o,"alsa_output.pci.hdmi-stereo",false);QCOMPARE(result["kind"].toString(),QString("Monitor · HDMI"));QVERIFY(result["default"].toBool());QCOMPARE(result["volume"].toInt(),50);QCOMPARE(result["channels"].toList().size(),2);QCOMPARE(result["sampleSpec"].toString(),QString("s32le 2ch 48000Hz"));
 }
 void missingDevicesCannotBecomeCommands(){
  Audio audio;audio.selectOutput("--help");audio.selectInput("fake");audio.volume("fake; rm -rf /",90);audio.streamVolume(999,70);audio.profile("fake","off");QCOMPARE(audio.findChildren<QProcess*>().size(),0);
 }
};
QTEST_GUILESS_MAIN(AudioTests)
#include "audio.moc"
