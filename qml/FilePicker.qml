import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel

// Sovel's file picker: Open, Save, Save As and Select Folder for apps that use
// the XDG portal. Colors come from the account's appearance, so Private Mode
// shows the dark laboratory palette and Normal Mode the graphite one.
Rectangle {
 id:root
 width:980;height:640
 color:SettingsService.colors.background
 readonly property var c:SettingsService.colors
 readonly property bool saving:Picker.kind==="save"
 readonly property bool choosingFolder:Picker.kind==="folder"||Picker.kind==="save-files"
 property string folder:Picker.startFolder
 property var history:[]
 property int filterIndex:Math.max(0,Picker.currentFilter)
 property string query:""
 property bool showHidden:false
 property var chosen:[]          // selected paths
 property string message:""
 readonly property bool recentView:folder==="recent:"
 readonly property var activeFilter:Picker.filters.length?Picker.filters[Math.min(filterIndex,Picker.filters.length-1)]:null

 function go(path){
  if(!path||path===folder)return
  if(path!=="recent:"&&!Picker.isFolder(path)){message="That folder is not available.";return}
  history=history.concat([folder]);folder=path;chosen=[];query="";search.text="";message=""
 }
 function back(){if(!history.length)return;folder=history[history.length-1];history=history.slice(0,-1);chosen=[]}
 function up(){if(!recentView)go(Picker.parentOf(folder))}
 function toggle(path,additive){
  if(Picker.multiple&&additive)chosen=chosen.includes(path)?chosen.filter(p=>p!==path):chosen.concat([path])
  else chosen=[path]
 }
 function finishSave(force){
  const name=nameField.text.trim()
  if(!name){message="Type a file name.";nameField.forceActiveFocus();return}
  const target=Picker.expand(name,recentView?Picker.startFolder:folder)
  if(Picker.isFolder(target)){go(target);nameField.text="";return}
  if(!Picker.isFolder(Picker.parentOf(target))){message="That folder does not exist.";return}
  if(!force&&Picker.exists(target)){overwrite.target=target;overwrite.open();return}
  Picker.accept([target],filterIndex)
 }
 function acceptNow(){
  message=""
  if(saving){finishSave(false);return}
  if(choosingFolder){Picker.accept([chosen.length&&Picker.isFolder(chosen[0])?chosen[0]:folder],-1);return}
  if(!chosen.length){message="Choose a file first.";return}
  if(chosen.length===1&&Picker.isFolder(chosen[0])){go(chosen[0]);return}
  Picker.accept(chosen.filter(p=>!Picker.isFolder(p)),filterIndex)
 }
 function sizeText(bytes){
  if(bytes<1024)return bytes+" B"
  const units=["KB","MB","GB","TB"];let value=bytes/1024,unit=0
  while(value>=1024&&unit<units.length-1){value/=1024;unit++}
  return (value<10?value.toFixed(1):Math.round(value))+" "+units[unit]
 }
 Component.onCompleted:{if(saving){nameField.text=Picker.currentName;const dot=Picker.currentName.lastIndexOf(".");nameField.select(0,dot>0?dot:Picker.currentName.length);nameField.forceActiveFocus()}else list.forceActiveFocus()}
 Shortcut {sequence:"Escape";onActivated:Picker.cancel()}
 Shortcut {sequence:"Ctrl+L";onActivated:{location.visible=true;location.text=Picker.displayPath(root.folder)==="Home"?"~/":root.folder+"/";location.forceActiveFocus()}}
 Shortcut {sequence:"Ctrl+H";onActivated:root.showHidden=!root.showHidden}
 Shortcut {sequence:"Ctrl+F";onActivated:search.forceActiveFocus()}
 Shortcut {sequence:"Alt+Up";onActivated:root.up()}
 Shortcut {sequence:"Alt+Left";onActivated:root.back()}

 FolderListModel {
  id:entries
  folder:root.recentView?"":"file://"+root.folder.split("/").map(encodeURIComponent).join("/")
  showDirsFirst:true;showDotAndDotDot:false;showHidden:root.showHidden;caseSensitive:false
  showFiles:!root.choosingFolder
  sortCaseSensitive:false
  nameFilters:root.query?["*"+root.query+"*"]:root.activeFilter&&!root.choosingFolder?root.activeFilter.patterns:[]
 }

 RowLayout {
  anchors.fill:parent;spacing:0
  // Places, drives and bookmarks.
  Rectangle {
   Layout.fillHeight:true;Layout.preferredWidth:214;color:root.c.sidebar
   ScrollView {
    anchors.fill:parent;anchors.margins:10;clip:true
    Column {
     width:194;spacing:2
     Text {text:"Places";color:root.c.muted;font.pixelSize:11;font.bold:true;leftPadding:8;bottomPadding:4;topPadding:4}
     Repeater {model:Picker.places;delegate:place}
     Text {visible:Picker.drives.length>0;text:"Drives";color:root.c.muted;font.pixelSize:11;font.bold:true;leftPadding:8;topPadding:12;bottomPadding:4}
     Repeater {model:Picker.drives;delegate:place}
    }
   }
  }
  Rectangle {Layout.fillHeight:true;width:1;color:root.c.border}
  ColumnLayout {
   Layout.fillWidth:true;Layout.fillHeight:true;spacing:0
   // Navigation, path and search.
   RowLayout {
    Layout.fillWidth:true;Layout.margins:12;spacing:8
    SovelButton {text:"‹";implicitWidth:38;enabled:root.history.length>0;onClicked:root.back();Accessible.name:"Back"}
    SovelButton {text:"↑";implicitWidth:38;enabled:!root.recentView&&root.folder!=="/";onClicked:root.up();Accessible.name:"Up one folder"}
    Item {
     Layout.fillWidth:true;implicitHeight:38
     Flickable {
      id:crumbs;anchors.fill:parent;visible:!location.visible;clip:true
      contentWidth:crumbRow.width;flickableDirection:Flickable.HorizontalFlick
      onContentWidthChanged:contentX=Math.max(0,contentWidth-width)
      Row {
       id:crumbRow;height:parent.height;spacing:2
       Repeater {
        model:{
         if(root.recentView)return [{name:"Recent",path:"recent:"}]
         const parts=root.folder.split("/").filter(p=>p);const out=[{name:"Computer",path:"/"}];let path=""
         for(const part of parts){path+="/"+part;out.push({name:part,path:path})}
         return out
        }
        delegate:Button {
         required property var modelData
         height:34;flat:true;hoverEnabled:true
         onClicked:root.go(modelData.path)
         onDoubleClicked:{location.visible=true;location.text=root.folder+"/";location.forceActiveFocus()}
         contentItem:Text {text:parent.modelData.name+"  ›";color:parent.modelData.path===root.folder?root.c.text:root.c.muted;font.pixelSize:13;verticalAlignment:Text.AlignVCenter}
         background:Rectangle {radius:8;color:parent.hovered?root.c.surfaceHover:"transparent"}
        }
       }
      }
     }
     TextField {
      id:location;anchors.fill:parent;visible:false;color:root.c.text;placeholderText:"Type a folder path";placeholderTextColor:root.c.muted
      background:Rectangle {radius:10;color:root.c.field;border.color:root.c.accent}
      onAccepted:{const path=Picker.expand(text,root.folder);visible=false;if(Picker.isFolder(path))root.go(path);else if(root.saving){nameField.text=path;root.finishSave(false)}else root.message="That folder does not exist."}
      onActiveFocusChanged:if(!activeFocus)visible=false
      Keys.onEscapePressed:visible=false
     }
    }
    TextField {
     id:search;Layout.preferredWidth:200;placeholderText:"Search this folder";placeholderTextColor:root.c.muted;color:root.c.text
     background:Rectangle {radius:10;color:root.c.field;border.color:search.activeFocus?root.c.accent:root.c.border}
     onTextChanged:searchDelay.restart()
     Timer {id:searchDelay;interval:180;onTriggered:root.query=search.text.trim().replace(/[\[\]*?]/g,"")}
     Accessible.name:"Search this folder"
    }
    SovelButton {text:"New Folder";enabled:!root.recentView;onClicked:newFolder.open()}
   }
   Rectangle {Layout.fillWidth:true;height:1;color:root.c.border}
   // Column headings.
   RowLayout {
    Layout.fillWidth:true;Layout.leftMargin:16;Layout.rightMargin:16;Layout.topMargin:6;spacing:10
    Text {Layout.fillWidth:true;text:"Name";color:root.c.muted;font.pixelSize:11}
    Text {Layout.preferredWidth:90;text:"Size";color:root.c.muted;font.pixelSize:11;horizontalAlignment:Text.AlignRight}
    Text {Layout.preferredWidth:150;text:"Modified";color:root.c.muted;font.pixelSize:11}
   }
   ListView {
    id:list
    Layout.fillWidth:true;Layout.fillHeight:true;Layout.margins:8;clip:true;spacing:1
    model:root.recentView?Picker.recent.filter(f=>!root.query||f.name.toLowerCase().includes(root.query.toLowerCase())):entries
    keyNavigationEnabled:true;focus:true;currentIndex:-1
    onModelChanged:currentIndex=-1
    ScrollBar.vertical:ScrollBar {}
    highlightMoveDuration:0
    delegate:Rectangle {
     id:row
     required property int index
     required property var model
     readonly property string path:root.recentView?model.modelData.path:model.filePath
     readonly property string name:root.recentView?model.modelData.name:model.fileName
     readonly property bool folderEntry:root.recentView?model.modelData.folder:model.fileIsDir
     readonly property bool picked:root.chosen.includes(path)
     width:ListView.view.width;height:36;radius:8
     color:picked?Qt.tint(root.c.surface,"#55"+String(root.c.accent).slice(1)):hover.hovered?root.c.surfaceHover:"transparent"
     HoverHandler {id:hover}
     RowLayout {
      anchors.fill:parent;anchors.leftMargin:8;anchors.rightMargin:8;spacing:10
      Text {text:row.folderEntry?"▸":"•";color:row.folderEntry?root.c.accent:root.c.muted;font.pixelSize:13;Layout.preferredWidth:12}
      Text {Layout.fillWidth:true;text:row.name;color:root.c.text;elide:Text.ElideMiddle;font.pixelSize:13;textFormat:Text.PlainText}
      Text {Layout.preferredWidth:90;horizontalAlignment:Text.AlignRight;text:row.folderEntry?"":root.sizeText(root.recentView?row.model.modelData.size:row.model.fileSize);color:root.c.muted;font.pixelSize:12}
      Text {Layout.preferredWidth:150;text:Qt.formatDateTime(root.recentView?row.model.modelData.modified:row.model.fileModified,"MMM d yyyy, h:mm AP");color:root.c.muted;font.pixelSize:12;elide:Text.ElideRight}
     }
     MouseArea {
      anchors.fill:parent
      onClicked:mouse=>{list.currentIndex=row.index;root.toggle(row.path,mouse.modifiers&Qt.ControlModifier);if(root.saving&&!row.folderEntry)nameField.text=row.name}
      onDoubleClicked:{if(row.folderEntry)root.go(row.path);else if(root.saving){nameField.text=row.name;root.finishSave(false)}else if(!root.choosingFolder){root.chosen=[row.path];root.acceptNow()}}
     }
    }
    Keys.onReturnPressed:{if(currentItem)root.chosen=[currentItem.path];root.acceptNow()}
    Keys.onPressed:event=>{if(event.key===Qt.Key_Backspace&&!root.saving){root.up();event.accepted=true}}
    onCurrentIndexChanged:if(currentIndex>=0&&currentItem&&!root.saving)root.chosen=[currentItem.path]
    Text {
     anchors.centerIn:parent;visible:list.count===0
     text:root.query?"Nothing here matches “"+root.query+"”.":root.recentView?"No recent files.":root.choosingFolder?"No folders here. Choose this folder, or create one.":"This folder is empty."
     color:root.c.muted;font.pixelSize:13
    }
   }
   Rectangle {Layout.fillWidth:true;height:1;color:root.c.border}
   // Name, file type and the decision.
   ColumnLayout {
    Layout.fillWidth:true;Layout.margins:12;spacing:8
    RowLayout {
     Layout.fillWidth:true;spacing:10;visible:root.saving
     Text {text:"Name";color:root.c.text;font.pixelSize:13}
     TextField {
      id:nameField;Layout.fillWidth:true;color:root.c.text;selectByMouse:true
      background:Rectangle {radius:10;color:root.c.field;border.color:nameField.activeFocus?root.c.accent:root.c.border}
      onAccepted:root.finishSave(false)
      Accessible.name:"File name"
     }
    }
    RowLayout {
     Layout.fillWidth:true;spacing:10
     Text {
      Layout.fillWidth:true;elide:Text.ElideMiddle;font.pixelSize:12
      text:root.message||(root.saving?"Saving to "+Picker.displayPath(root.recentView?Picker.startFolder:root.folder):root.choosingFolder?(Picker.kind==="save-files"?"Saving "+Picker.saveNames.length+" files to ":"Selecting ")+Picker.displayPath(root.chosen.length&&Picker.isFolder(root.chosen[0])?root.chosen[0]:root.folder):root.chosen.length>1?root.chosen.length+" files selected":root.chosen.length?Picker.displayPath(root.chosen[0]):"")
      color:root.message?root.c.danger:root.c.muted
     }
     CheckBox {text:"Hidden files";checked:root.showHidden;onToggled:root.showHidden=checked;palette.windowText:root.c.muted}
     SovelCombo {visible:Picker.filters.length>0&&!root.choosingFolder;model:Picker.filters;textRole:"name";currentIndex:root.filterIndex;onActivated:root.filterIndex=currentIndex}
     SovelButton {text:"Cancel";onClicked:Picker.cancel()}
     SovelButton {text:Picker.acceptLabel;primary:true;enabled:root.saving||root.choosingFolder||root.chosen.length>0;onClicked:root.acceptNow()}
    }
   }
  }
 }
 Component {
  id:place
  Button {
   required property var modelData
   width:194;height:34;flat:true;hoverEnabled:true
   onClicked:root.go(modelData.path)
   contentItem:RowLayout {spacing:9
    SovelSymbol {Layout.preferredWidth:15;Layout.preferredHeight:15;symbol:modelData.symbol;color:modelData.path===root.folder?root.c.accent:root.c.muted}
    Text {Layout.fillWidth:true;text:modelData.name;color:root.c.text;elide:Text.ElideRight;font.pixelSize:13;textFormat:Text.PlainText}
   }
   background:Rectangle {radius:9;color:modelData.path===root.folder?Qt.tint(root.c.surface,"#40"+String(root.c.accent).slice(1)):parent.hovered?root.c.surfaceHover:"transparent"}
  }
 }
 Dialog {
  id:newFolder;title:"New folder";modal:true;anchors.centerIn:parent;width:380;standardButtons:Dialog.Ok|Dialog.Cancel
  background:Rectangle {radius:14;color:root.c.surface;border.color:root.c.border}
  contentItem:ColumnLayout {spacing:10
   TextField {id:folderName;Layout.fillWidth:true;placeholderText:"Folder name";color:root.c.text;onAccepted:newFolder.accept()}
   Text {id:folderError;Layout.fillWidth:true;visible:!!text;color:root.c.danger;wrapMode:Text.Wrap;font.pixelSize:12}
  }
  onOpened:{folderName.text="New Folder";folderName.selectAll();folderName.forceActiveFocus();folderError.text=""}
  onAccepted:{const error=Picker.makeFolder(root.folder,folderName.text);if(error){folderError.text=error;open();return}root.go(Picker.join(root.folder,folderName.text.trim()))}
 }
 Dialog {
  id:overwrite;property string target:"";title:"Replace existing file?";modal:true;anchors.centerIn:parent;width:420;standardButtons:Dialog.Cancel
  background:Rectangle {radius:14;color:root.c.surface;border.color:root.c.border}
  contentItem:Text {text:"“"+overwrite.target.split("/").pop()+"” already exists in this folder. Replacing it overwrites its contents.";wrapMode:Text.Wrap;color:root.c.text}
  footer:DialogButtonBox {
   background:Item {}
   SovelButton {text:"Cancel";DialogButtonBox.buttonRole:DialogButtonBox.RejectRole}
   SovelButton {text:"Replace";primary:true;DialogButtonBox.buttonRole:DialogButtonBox.AcceptRole}
  }
  onAccepted:Picker.accept([target],root.filterIndex)
 }
 Timer {interval:4000;repeat:true;running:true;onTriggered:Picker.refreshDrives()}
}
