import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel

Rectangle {
 id:root
 property url initialFolder
 radius:12
 color:SettingsService.colors.surface
 border.color:SettingsService.colors.border
 FolderListModel {
  id:folders
  folder:root.initialFolder
  showHidden:Browser.showHidden
  showDirsFirst:true
 }
 ColumnLayout {
  anchors.fill:parent
  anchors.margins:10
  RowLayout {
   Layout.fillWidth:true
   SovelButton {text:"↑";Accessible.name:"Parent folder in split pane";enabled:folders.parentFolder.toString()!=="";onClicked:folders.folder=folders.parentFolder}
   SovelButton {text:"Follow Main";onClicked:folders.folder=root.initialFolder}
  }
  SettingsLabel {Layout.fillWidth:true;text:decodeURIComponent(folders.folder.toString().replace(/^file:\/\//,""));elide:Text.ElideMiddle;color:SettingsService.colors.muted}
  ListView {
   id:entries
   Layout.fillWidth:true
   Layout.fillHeight:true
   model:folders
   clip:true
   delegate:ItemDelegate {
    required property string fileName
    required property url fileUrl
    required property bool fileIsDir
    width:entries.width
    height:33
    text:(fileIsDir?"▸  ":"")+fileName
    onClicked:if(fileIsDir)folders.folder=fileUrl
    onDoubleClicked:if(!fileIsDir)Browser.openLocation(fileUrl)
   }
   ScrollBar.vertical:ScrollBar {}
  }
 }
}
