import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Pdf
import QtMultimedia

ColumnLayout {
 id:root
 required property var details
 property bool playing:false
 readonly property string mime:details.mime||""
 SettingsLabel {Layout.fillWidth:true;text:root.details.name||"";font.pixelSize:19;font.bold:true;elide:Text.ElideMiddle}
 SettingsLabel {Layout.fillWidth:true;text:[root.details.type,root.details.bytes,root.details.dimensions,root.details.modified].filter(Boolean).join(" · ");wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 Loader {
  Layout.fillWidth:true
  Layout.fillHeight:true
  sourceComponent:root.mime.startsWith("image/")?imagePreview:root.mime==="application/pdf"?pdfPreview:
                  root.mime.startsWith("video/")||root.mime.startsWith("audio/")?mediaPreview:
                  root.details.text!==undefined?textPreview:info
 }
 Component {
  id:imagePreview
  Item {
   Image {
    id:photo
    anchors.fill:parent
    source:root.details.url
    asynchronous:true
    fillMode:Image.PreserveAspectFit
    sourceSize:Qt.size(1400,1400)
   }
   SettingsLabel {anchors.centerIn:parent;visible:photo.status===Image.Error;text:"This image could not be previewed."}
  }
 }
 Component {
  id:pdfPreview
  PdfMultiPageView {
   document:PdfDocument {source:root.details.url}
   renderScale:1
   clip:true
  }
 }
 Component {
  id:mediaPreview
  ColumnLayout {
   VideoOutput {id:video;Layout.fillWidth:true;Layout.fillHeight:true}
   MediaPlayer {
    id:player
    source:root.details.url
    audioOutput:AudioOutput {}
    videoOutput:video
   }
   RowLayout {
    SovelButton {text:player.playbackState===MediaPlayer.PlayingState?"Pause":"Play";onClicked:player.playbackState===MediaPlayer.PlayingState?player.pause():player.play()}
    Slider {Layout.fillWidth:true;from:0;to:Math.max(1,player.duration);value:player.position;enabled:player.seekable;onMoved:player.position=value}
   }
   SettingsLabel {Layout.fillWidth:true;text:player.errorString;visible:!!text;wrapMode:Text.WordWrap}
  }
 }
 Component {
  id:textPreview
  ScrollView {
   clip:true
   Row {
    spacing:10
    Text {text:String(root.details.text).split("\n").map((_,index)=>String(index+1)).join("\n");font.family:"monospace";font.pixelSize:13;color:SettingsService.colors.muted}
    TextEdit {
     text:root.details.text
     font.family:"monospace"
     font.pixelSize:13
     color:SettingsService.colors.text
     readOnly:true
     selectByMouse:true
     textFormat:TextEdit.PlainText
    }
   }
  }
 }
 Component {
  id:info
  SettingsLabel {text:"Preview is unavailable for this file type.";color:SettingsService.colors.muted;wrapMode:Text.WordWrap}
 }
 SettingsLabel {Layout.fillWidth:true;visible:!!root.details.encoding;text:root.details.encoding+(root.details.truncated?" · First 96 KiB":"");color:SettingsService.colors.muted}
 SettingsLabel {Layout.fillWidth:true;text:"Owner: "+(root.details.owner||"Unknown")+" · Group: "+(root.details.group||"Unknown")+" · "+(root.details.writable?"Writable":root.details.readable?"Read only":"Unavailable");wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 SettingsLabel {Layout.fillWidth:true;visible:!!root.details.symlink;text:"Link target: "+root.details.symlink;wrapMode:Text.WrapAnywhere}
}
