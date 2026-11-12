import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Settings → Apps & Defaults: where downloads go, and the most recent ones.
SettingsCard {
 id:panel
 SettingsLabel {text:"Downloads";font.bold:true;font.pixelSize:17}
 RowLayout {
  Layout.fillWidth:true;spacing:10
  ColumnLayout {Layout.fillWidth:true;spacing:2
   SettingsLabel {text:"Default folder"}
   SettingsLabel {Layout.fillWidth:true;text:DownloadsHub.folder.replace(System.home,"~");color:SettingsService.colors.muted;elide:Text.ElideMiddle}
  }
  SovelButton {text:"Change…";onClicked:folderPicker.open()}
  SovelButton {text:"Open";onClicked:DownloadsHub.openFolder()}
 }
 SettingsLabel {Layout.fillWidth:true;visible:!!DownloadsHub.status;text:DownloadsHub.status;color:SettingsService.colors.danger;wrapMode:Text.WordWrap}
 SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:SettingsService.colors.muted
  text:"Apps that save through the Sovel file picker start in this folder. Browsers use it when they are set to save without asking."}
 SettingsLabel {visible:DownloadsHub.items.length>0;text:"Recent";font.bold:true}
 Repeater {
  model:DownloadsHub.items.slice(0,5)
  RowLayout {required property var modelData;Layout.fillWidth:true;spacing:8
   SettingsLabel {Layout.fillWidth:true;text:(modelData.partial?"Downloading · ":"")+modelData.name;elide:Text.ElideMiddle}
   SovelButton {visible:!modelData.partial;text:"Show";onClicked:DownloadsHub.showInFolder(modelData.path)}
  }
 }
 FolderDialog {
  id:folderPicker;title:"Choose the Downloads folder"
  currentFolder:"file://"+DownloadsHub.folder
  onAccepted:DownloadsHub.setFolder(selectedFolder.toString())
 }
}
