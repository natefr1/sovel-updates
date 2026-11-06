import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
Item {
 id:root;width:1120;height:740
 readonly property bool listMode:Browser.viewMode==="list"||Browser.viewMode==="compact"
 property bool editingPath:false
 property var selected:[]
 property string query:""
 property string kindFilter:"all"
 property var activeVolume:({})
 property var previewDetails:({})
 property bool splitView:false
 readonly property var selectedEntry:selected.length===1?item(selected[0]):null
 readonly property var rows:Browser.entries.filter(e=>(!query||e.name.toLowerCase().includes(query.toLowerCase()))&&
   (kindFilter==="all"||kindFilter==="folder"&&e.folder||kindFilter==="archive"&&e.archive||String(e.mime).startsWith(kindFilter)))
 readonly property bool inFolder:Browser.collection===""
 readonly property string folderName:!inFolder?Browser.collection:Browser.inTrash?"Trash":Browser.path===System.home?"Home":Browser.path.split('/').filter(Boolean).pop()||"Computer"
 readonly property var breadcrumbs:{
  if(Browser.inTrash||!inFolder)return [{name:folderName,path:Browser.path}]
  const result=[{name:"System Drive",path:"/"}]
  let path=""
  for(const part of Browser.path.split("/").filter(Boolean)){
   path+="/"+part
   result.push({name:path===System.home?"Home":part,path:path})
  }
  return result
 }
 function select(path,modifiers){if(modifiers&Qt.ControlModifier){selected=selected.includes(path)?selected.filter(p=>p!==path):selected.concat([path])}else selected=[path]}
 function item(path){return Browser.entries.find(e=>e.path===path)}
 function open(){
  if(!selectedEntry)return
  if(selectedEntry.archive){archiveDialog.open();Browser.archiveAction("preview",selectedEntry.path,"",false,"")}
  else Browser.open(selected[0])
 }
 function selectDrive(volume){
  if(volume.mount)Browser.volumeAction("open",volume.id,"")
  else {activeVolume=volume;driveDialog.open()}
 }
 function rename(){if(selected.length!==1)return;nameDialog.renaming=true;nameField.text=item(selected[0]).name;nameDialog.open()}
 function trash(){if(selected.length)trashDialog.open()}
 Component.onCompleted:root.forceActiveFocus()
 Connections {target:Browser;function onPathChanged(){root.selected=[];search.clear();root.editingPath=false}function onChanged(){root.selected=root.selected.filter(path=>Browser.entries.some(e=>e.path===path))}}
 Connections {target:sovelWindow;function onClosing(event){if(Browser.busy)event.accepted=false}}
 Rectangle {anchors.fill:parent;color:SettingsService.colors.background}
 component Label:Text {color:SettingsService.colors.text;textFormat:Text.PlainText;elide:Text.ElideRight;font.pixelSize:13}
 component Tool:SovelButton {implicitHeight:34;implicitWidth:Math.max(34,contentItem.implicitWidth+22)}
 Rectangle {
  id:sidebar;width:root.width<800?166:210;anchors.top:parent.top;anchors.bottom:parent.bottom;color:SettingsService.colors.sidebar
  ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:10
   Label {text:"Sovel Files";font.pixelSize:23;font.weight:Font.DemiBold;Layout.topMargin:6;Layout.bottomMargin:10}
   Label {text:"FAVORITES";font.pixelSize:9;font.letterSpacing:1.2;color:SettingsService.colors.muted}
   ScrollView {id:places;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
    Column {width:places.availableWidth;spacing:5
     Repeater {model:Browser.places
      Button {required property var modelData;width:parent.width;height:36;text:modelData.name
       onClicked:Browser.navigate(modelData.path)
       background:Rectangle {radius:8;color:Browser.path===parent.modelData.path?SettingsService.colors.surfaceHover:parent.hovered?SettingsService.colors.surface:"transparent"}
       contentItem:RowLayout {spacing:10
        Image {source:"image://apps/"+modelData.icon;Layout.preferredWidth:20;Layout.preferredHeight:20;sourceSize:Qt.size(40,40)}
        Label {text:modelData.name;Layout.fillWidth:true}
       }
      }
     }
     Label {text:"DEVICES";font.pixelSize:9;font.letterSpacing:1.2;color:SettingsService.colors.muted;topPadding:12}
     Repeater {model:Browser.volumes
      Button {required property var modelData;width:parent.width;height:44
       Accessible.name:modelData.name+(modelData.mount?"":" · Not mounted")
       onClicked:root.selectDrive(modelData)
       background:Rectangle {radius:8;color:parent.hovered?SettingsService.colors.surface:"transparent"}
       contentItem:RowLayout {
        Image {source:"image://apps/"+(modelData.removable?"drive-removable-media":"drive-harddisk");Layout.preferredWidth:20;Layout.preferredHeight:20}
        ColumnLayout {Layout.fillWidth:true;spacing:1
         Label {text:modelData.name;Layout.fillWidth:true}
         Label {text:modelData.mount?(modelData.readonly?"Read only":"Mounted"):"Not mounted";color:SettingsService.colors.muted;font.pixelSize:10}
        }
        Tool {text:"…";implicitWidth:28;Accessible.name:"Drive details";onClicked:{root.activeVolume=modelData;driveDialog.open()}}
       }
      }
     }
     Label {width:parent.width;text:Browser.volumeError;visible:text!=="";font.pixelSize:10;wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
    }
   }
   Tool {text:"Preferences";Layout.fillWidth:true;onClicked:preferencesDialog.open()}
  }
 }
 ColumnLayout {
  anchors.left:sidebar.right;anchors.top:parent.top;anchors.right:splitPane.active?splitPane.left:parent.right;anchors.bottom:parent.bottom;anchors.margins:16;spacing:12
  Flickable {Layout.fillWidth:true;Layout.preferredHeight:34;contentWidth:tabButtons.width;clip:true
   Row {id:tabButtons;spacing:4
    Repeater {model:Browser.tabs
     Row {required property var modelData;required property int index;spacing:1
      Tool {text:modelData.name;primary:Browser.activeTab===index;onClicked:Browser.switchTab(index)}
      Tool {text:"×";visible:Browser.tabs.length>1;implicitWidth:26;Accessible.name:"Close tab";onClicked:Browser.closeTab(index)}
     }
    }
    Tool {text:"+";Accessible.name:"New tab";onClicked:Browser.newTab()}
   }
  }
  RowLayout {Layout.fillWidth:true;spacing:7
   Tool {text:"‹";enabled:Browser.canBack;onClicked:Browser.back();Accessible.name:"Back"}
   Tool {text:"›";enabled:Browser.canForward;onClicked:Browser.forward();Accessible.name:"Forward"}
   Tool {text:"↑";enabled:Browser.path!=="/";onClicked:Browser.up();Accessible.name:"Parent folder"}
   Label {text:root.folderName;Layout.fillWidth:true;font.pixelSize:22;font.weight:Font.DemiBold;visible:!root.editingPath}
   TextField {id:pathField;visible:root.editingPath;Layout.fillWidth:true;text:Browser.path;color:"#F4E8FF";selectByMouse:true;onAccepted:Browser.navigate(text);Keys.onEscapePressed:root.editingPath=false;Accessible.name:"Folder location"}
   TextField {id:search;Layout.preferredWidth:root.width<850?145:210;placeholderText:"Search this folder";color:"#EEE4F8";placeholderTextColor:"#AD99BD";selectByMouse:true;onTextChanged:root.query=text
    background:Rectangle {radius:9;color:"#50100A16";border.color:search.activeFocus?"#B98CE0":"#3D6C507C"}
    Accessible.name:"Search this folder"
   }
  }
  RowLayout {Layout.fillWidth:true;spacing:8
   Tool {text:"New Folder";visible:!splitPane.active;enabled:root.inFolder&&!Browser.inTrash&&!Browser.busy;onClicked:{nameDialog.renaming=false;nameField.text="";nameDialog.open()}}
   Tool {text:"Copy";visible:root.width>850&&!splitPane.active;enabled:root.selected.length>0&&!Browser.busy;onClicked:Browser.copy(root.selected,false)}
   Tool {text:Browser.inTrash?"Restore":"Paste";enabled:root.inFolder&&!Browser.busy&&(!Browser.inTrash||root.selected.length===1);onClicked:Browser.inTrash?Browser.restore(root.selected[0]):Browser.paste()}
   Item {Layout.fillWidth:true}
   ComboBox {id:sortBox;model:["Name","Date modified","Size","Kind"];currentIndex:["name","modified","size","type"].indexOf(Browser.sort);implicitWidth:120;implicitHeight:34;onActivated:Browser.sort=["name","modified","size","type"][currentIndex];Accessible.name:"Sort files"
    background:Rectangle {radius:9;color:"#342542";border.color:sortBox.activeFocus?"#D8B2FF":"#605078"}
    contentItem:Label {text:sortBox.currentText;leftPadding:11;rightPadding:22;verticalAlignment:Text.AlignVCenter}
    indicator:Label {text:"⌄";x:sortBox.width-width-9;y:(sortBox.height-height)/2;color:"#D8B7EE"}
    delegate:ItemDelegate {width:sortBox.width;implicitHeight:34;highlighted:sortBox.highlightedIndex===index;contentItem:Label {text:modelData;verticalAlignment:Text.AlignVCenter} background:Rectangle {radius:6;color:parent.highlighted?"#745094":"#2A1C36"}}
    popup:Popup {y:sortBox.height+5;width:sortBox.width;padding:4;implicitHeight:contentItem.implicitHeight+8;background:ShellGlass {radius:10;tint:0.65}
     contentItem:ListView {clip:true;implicitHeight:contentHeight;model:sortBox.popup.visible?sortBox.delegateModel:null;currentIndex:sortBox.highlightedIndex}
    }
   }
   Tool {text:"View";onClicked:viewMenu.popup();Accessible.name:"Change file view"}
   Tool {text:Browser.path===Browser.downloadsPath()?"Downloads":"Transfers";onClicked:{transferDialog.downloads=Browser.path===Browser.downloadsPath();transferDialog.open()}}
   Tool {text:"•••";onClicked:folderMenu.popup();Accessible.name:"Folder options"}
  }
  RowLayout {Layout.fillWidth:true;spacing:6
   Flickable {Layout.fillWidth:true;Layout.preferredHeight:32;contentWidth:crumbs.width;clip:true
    Row {id:crumbs;spacing:3
     Repeater {model:root.breadcrumbs
      Tool {required property var modelData;text:modelData.name;implicitHeight:28;onClicked:Browser.navigate(modelData.path)}
     }
    }
   }
   Tool {text:"Location";onClicked:{root.editingPath=true;pathField.forceActiveFocus();pathField.selectAll()}}
   Tool {text:Browser.deepSearch?"Clear Search":"Find";enabled:root.inFolder&&!Browser.inTrash;onClicked:Browser.deepSearch?Browser.clearSearch():deepSearchDialog.open()}
   SovelCombo {model:["All types","Folders","Images","Video","Audio","Text","Archives"];implicitWidth:118
    onActivated:root.kindFilter=["all","folder","image/","video/","audio/","text/","archive"][currentIndex]}
   BusyIndicator {running:Browser.loading||Browser.busy;visible:running;Layout.preferredHeight:22;Layout.preferredWidth:22}
  }
  Rectangle {
   Layout.fillWidth:true;Layout.fillHeight:true;radius:12;color:SettingsService.colors.surface;border.color:SettingsService.colors.border;clip:true
   MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton;onClicked:event=>{if(event.button===Qt.RightButton)folderMenu.popup();else root.selected=[]}}
   ColumnLayout {anchors.fill:parent;anchors.margins:6;spacing:4;visible:root.listMode
    RowLayout {Layout.fillWidth:true;Layout.preferredHeight:29;Layout.leftMargin:10;Layout.rightMargin:10;spacing:12
     Label {text:"Name";Layout.fillWidth:true;color:"#AB97BE";font.pixelSize:11}
     Label {text:"Modified";Layout.preferredWidth:150;visible:root.width>900;color:"#AB97BE";font.pixelSize:11}
     Label {text:"Kind";Layout.preferredWidth:110;visible:root.width>780;color:"#AB97BE";font.pixelSize:11}
     Label {text:"Size";Layout.preferredWidth:70;color:"#AB97BE";font.pixelSize:11;horizontalAlignment:Text.AlignRight}
    }
    ListView {id:list;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:root.rows;spacing:2;boundsBehavior:Flickable.StopAtBounds
     delegate:Rectangle {required property var modelData;width:list.width;height:Browser.viewMode==="compact"?30:43;radius:7;color:root.selected.includes(modelData.path)?SettingsService.colors.surfaceHover:rowHover.hovered?SettingsService.colors.field:"transparent"
      HoverHandler {id:rowHover}
      RowLayout {anchors.fill:parent;anchors.leftMargin:10;anchors.rightMargin:10;spacing:12
       Image {source:modelData.icon;Layout.preferredWidth:25;Layout.preferredHeight:25;sourceSize:Qt.size(50,50)}
       Label {text:modelData.name;Layout.fillWidth:true}
       Label {text:modelData.modified;Layout.preferredWidth:150;visible:root.width>900;color:"#B6A1C8";font.pixelSize:11}
       Label {text:modelData.type;Layout.preferredWidth:110;visible:root.width>780;color:"#B6A1C8";font.pixelSize:11}
       Label {text:modelData.sizeText;Layout.preferredWidth:70;color:"#B6A1C8";font.pixelSize:11;horizontalAlignment:Text.AlignRight}
      }
      MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton
       onClicked:event=>{root.forceActiveFocus();if(event.button===Qt.RightButton){if(!root.selected.includes(modelData.path))root.selected=[modelData.path];itemMenu.popup()}else root.select(modelData.path,event.modifiers)}
       onDoubleClicked:{root.selected=[modelData.path];root.open()}
      }
     }
     ScrollBar.vertical:ScrollBar {}
    }
   }
   GridView {id:grid;anchors.fill:parent;anchors.margins:8;visible:!root.listMode;clip:true;cellWidth:132;cellHeight:128;model:root.rows;boundsBehavior:Flickable.StopAtBounds
    delegate:Rectangle {required property var modelData;width:grid.cellWidth-8;height:grid.cellHeight-8;radius:10;color:root.selected.includes(modelData.path)?"#885F4182":gridHover.hovered?"#17FFFFFF":"transparent"
     HoverHandler {id:gridHover}
     Column {anchors.fill:parent;anchors.margins:10;spacing:9
      Image {anchors.horizontalCenter:parent.horizontalCenter;width:58;height:58;source:Browser.viewMode==="gallery"&&String(modelData.mime).startsWith("image/")?modelData.url:modelData.icon;asynchronous:true;fillMode:Image.PreserveAspectFit;sourceSize:Qt.size(116,116)}
      Label {width:parent.width;text:modelData.name;horizontalAlignment:Text.AlignHCenter;elide:Text.ElideMiddle;font.pixelSize:12}
     }
     MouseArea {anchors.fill:parent;acceptedButtons:Qt.LeftButton|Qt.RightButton
      onClicked:event=>{root.forceActiveFocus();if(event.button===Qt.RightButton){if(!root.selected.includes(modelData.path))root.selected=[modelData.path];itemMenu.popup()}else root.select(modelData.path,event.modifiers)}
      onDoubleClicked:{root.selected=[modelData.path];root.open()}
     }
    }
    ScrollBar.vertical:ScrollBar {}
   }
   Column {anchors.centerIn:parent;spacing:8;visible:root.rows.length===0&&!Browser.loading
    Label {anchors.horizontalCenter:parent.horizontalCenter;text:root.query?"No matching files":"This folder is empty";font.pixelSize:19;color:"#C4AED7"}
    Label {anchors.horizontalCenter:parent.horizontalCenter;text:root.query?"Try a different search.":"Add files or create a new folder.";font.pixelSize:12;color:"#9079A5"}
   }
  }
  RowLayout {Layout.fillWidth:true
   Label {text:(root.selected.length?root.selected.length+" selected · ":"")+root.rows.length+" items";color:"#AB95BD";font.pixelSize:11}
   Item {Layout.fillWidth:true}
   Label {text:Browser.busy&&Browser.operationProgress?Browser.operationProgress:Browser.status;visible:text!==Browser.entries.length+" items";Layout.maximumWidth:root.width-360;color:SettingsService.colors.muted;font.pixelSize:11;ToolTip.text:text;ToolTip.visible:statusHover.hovered;HoverHandler{id:statusHover}}
  }
  Tool {visible:Browser.archiveDestination!==""&&!Browser.busy;text:"Open result location";onClicked:Browser.openResultLocation()}
 }
 Loader {
  id:splitPane
  active:root.splitView&&root.width>=970
  anchors.right:parent.right
  anchors.top:parent.top
  anchors.bottom:parent.bottom
  anchors.margins:16
  width:Math.min(305,root.width*.27)
  sourceComponent:FilesSplitPane {initialFolder:"file://"+(root.inFolder?Browser.path:System.home).split("/").map(encodeURIComponent).join("/")}
 }
 ShellMenu {id:viewMenu
  ShellMenuItem {text:"Detailed list";onTriggered:Browser.viewMode="list"}
  ShellMenuItem {text:"Compact list";onTriggered:Browser.viewMode="compact"}
  ShellMenuItem {text:"Grid";onTriggered:Browser.viewMode="grid"}
  ShellMenuItem {text:"Gallery";onTriggered:Browser.viewMode="gallery"}
  ShellMenuItem {text:"Split View";checkable:true;checked:root.splitView;enabled:root.width>=970;onTriggered:root.splitView=!root.splitView}
 }
 ShellMenu {id:folderMenu
  ShellMenuItem {text:"New Folder";enabled:root.inFolder&&!Browser.inTrash;onTriggered:{nameDialog.renaming=false;nameField.clear();nameDialog.open()}}
  ShellMenuItem {text:"Paste";enabled:root.inFolder&&!Browser.inTrash&&!Browser.busy;onTriggered:Browser.paste()}
  ShellMenuItem {text:"Show Hidden Files";checkable:true;checked:Browser.showHidden;onTriggered:Browser.showHidden=!Browser.showHidden}
  ShellMenuItem {text:"Edit Location";onTriggered:{root.editingPath=true;pathField.forceActiveFocus();pathField.selectAll()}}
  ShellMenuItem {text:"Refresh";onTriggered:Browser.refresh()}
  ShellMenuItem {text:"Toggle favorite for this folder";enabled:root.inFolder&&!Browser.inTrash;onTriggered:Browser.favorite(Browser.path)}
  ShellMenuItem {text:"Download Manager";onTriggered:{transferDialog.downloads=true;transferDialog.open()}}
 }
 ShellMenu {id:itemMenu
  ShellMenuItem {text:"Restore";visible:Browser.inTrash;enabled:root.selected.length===1&&!Browser.busy;onTriggered:Browser.restore(root.selected[0])}
  ShellMenuItem {text:root.selected.length===1&&root.item(root.selected[0])&&root.item(root.selected[0]).package?"Install Package…":"Open";enabled:root.selected.length===1;onTriggered:root.open()}
  MenuSeparator {}
  ShellMenuItem {text:"Copy";onTriggered:Browser.copy(root.selected,false)}
  ShellMenuItem {text:"Cut";enabled:!Browser.inTrash;onTriggered:Browser.copy(root.selected,true)}
  ShellMenuItem {text:"Move To…";enabled:root.selected.length>0&&!Browser.inTrash;onTriggered:{movePicker.paths=root.selected.slice();movePicker.open()}}
  ShellMenuItem {text:"Rename…";enabled:root.selected.length===1&&!Browser.inTrash;onTriggered:root.rename()}
  ShellMenuItem {text:"Copy file path";enabled:!!root.selectedEntry;onTriggered:Browser.copyPath(root.selected[0])}
  ShellMenuItem {text:root.selectedEntry?.starred?"Remove Star":"Star";visible:!!root.selectedEntry&&!Browser.inTrash;onTriggered:Browser.toggleStar(root.selected[0])}
  ShellMenuItem {text:"Preview & Properties";enabled:!!root.selectedEntry;onTriggered:{root.previewDetails=Browser.inspect(root.selected[0]);previewDialog.open()}}
  ShellMenuItem {text:"Copy SHA-256";enabled:!!root.selectedEntry&&!root.selectedEntry.folder&&!Browser.busy;onTriggered:Browser.checksum(root.selected[0])}
  ShellMenuItem {text:"Duplicate";enabled:root.inFolder&&!!root.selectedEntry&&!Browser.busy&&!Browser.inTrash;onTriggered:Browser.duplicate(root.selected[0])}
  ShellMenuItem {text:"Create Shortcut";enabled:root.inFolder&&!!root.selectedEntry&&!Browser.busy&&!Browser.inTrash;onTriggered:Browser.shortcut(root.selected[0])}
  ShellMenuItem {text:"Toggle folder favorite";visible:!!root.selectedEntry&&root.selectedEntry.folder;onTriggered:Browser.favorite(root.selected[0])}
  ShellMenuItem {text:"Preview archive contents";visible:!!root.selectedEntry&&root.selectedEntry.archive;enabled:!Browser.busy;onTriggered:root.open()}
  ShellMenuItem {text:"Extract Here";visible:root.inFolder&&!!root.selectedEntry&&root.selectedEntry.archive;enabled:!Browser.busy;onTriggered:Browser.archiveAction("extract",root.selected[0],Browser.path,false,"")}
  ShellMenuItem {text:"Extract to New Folder";visible:root.inFolder&&!!root.selectedEntry&&root.selectedEntry.archive;enabled:!Browser.busy;onTriggered:Browser.archiveAction("extract",root.selected[0],Browser.path,true,"")}
  ShellMenuItem {text:"Extract To…";visible:!!root.selectedEntry&&root.selectedEntry.archive;enabled:!Browser.busy;onTriggered:{extractPicker.archivePath=root.selected[0];extractPicker.open()}}
  ShellMenuItem {text:"Test archive";visible:!!root.selectedEntry&&root.selectedEntry.archive;enabled:!Browser.busy;onTriggered:Browser.archiveAction("test",root.selected[0],"",false,"")}
  ShellMenuItem {text:"Compress…";enabled:root.selected.length>0&&!Browser.busy&&!Browser.inTrash;onTriggered:{archiveName.text="Archive";compressDialog.open()}}
  ShellMenuItem {text:"Move to Trash…";enabled:!Browser.busy&&!Browser.inTrash;onTriggered:root.trash()}
 }
 Dialog {id:previewDialog;title:"Preview";modal:true;anchors.centerIn:parent;width:Math.min(root.width-30,760);height:Math.min(root.height-30,600);standardButtons:Dialog.Close
  background:ShellGlass {radius:16;tint:.97}
  contentItem:Loader {active:previewDialog.visible;sourceComponent:FilesPreview {details:root.previewDetails}}
 }
 Dialog {id:transferDialog;property bool downloads:false;title:downloads?"Download Manager":"File Transfers";modal:true;anchors.centerIn:parent;width:Math.min(root.width-32,640);height:Math.min(root.height-32,490);standardButtons:Dialog.Close
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   RowLayout {visible:transferDialog.downloads;Layout.fillWidth:true
    Tool {text:"New Download";onClicked:downloadDialog.open()}
    Tool {text:"Show Downloaded Files";onClicked:{Browser.navigate(Browser.downloadsPath());transferDialog.close()}}
   }
   Label {visible:transferDialog.downloads;Layout.fillWidth:true;text:"This list shows downloads started in Sovel Files.";color:SettingsService.colors.muted}
   ListView {
   id:transferList;Layout.fillWidth:true;Layout.fillHeight:true;model:Browser.transfers.filter(job=>transferDialog.downloads?job.kind==="download":job.kind!=="download");clip:true;spacing:10
   delegate:ColumnLayout {required property var modelData;width:transferList.width;spacing:5
    readonly property bool active:["queued","running","paused","finishing"].includes(modelData.state)
    Label {Layout.fillWidth:true;text:(modelData.filename||(modelData.move?"Move":"Copy"))+" · "+modelData.state;font.bold:true}
    Label {Layout.fillWidth:true;text:modelData.destination;elide:Text.ElideMiddle;color:SettingsService.colors.muted}
    Label {visible:modelData.kind==="download";Layout.fillWidth:true;elide:Text.ElideRight;color:SettingsService.colors.muted
     text:["Sovel Files",modelData.mime,modelData.total>0?(modelData.total/1048576).toFixed(2)+" MiB":null,
           modelData.finished?Qt.formatDateTime(new Date(modelData.finished*1000),Qt.locale().dateTimeFormat(Locale.ShortFormat)):null].filter(Boolean).join(" · ")}
    ProgressBar {visible:parent.active;Layout.fillWidth:true;from:0;to:Math.max(1,modelData.total||0);value:modelData.bytes||0;indeterminate:!(modelData.total>0)}
    Label {Layout.fillWidth:true;visible:!!text;text:modelData.error||(modelData.speed>0?(modelData.speed/1048576).toFixed(1)+" MiB/s · "+modelData.eta+" s remaining":"");wrapMode:Text.WordWrap}
    RowLayout {
     Tool {text:modelData.state==="paused"?"Resume":"Pause";visible:["running","paused"].includes(modelData.state);onClicked:Browser.transferAction(modelData.id,modelData.state==="paused"?"resume":"pause")}
     Tool {text:"Cancel";visible:["queued","running","paused"].includes(modelData.state);onClicked:Browser.transferAction(modelData.id,"cancel")}
     Tool {text:"Retry";visible:["failed","cancelled","interrupted"].includes(modelData.state);onClicked:Browser.transferAction(modelData.id,"retry")}
     Tool {text:"Show Destination";onClicked:{Browser.transferAction(modelData.id,"open");transferDialog.close()}}
     Tool {text:"Open";visible:modelData.state==="complete"&&modelData.kind==="download";onClicked:Browser.transferAction(modelData.id,"open-file")}
     Tool {text:"Clear";visible:!parent.parent.active;onClicked:Browser.transferAction(modelData.id,"clear")}
    }
   }
   Label {anchors.centerIn:parent;visible:transferList.count===0;text:transferDialog.downloads?"No Sovel downloads yet.":"No transfers yet."}
   ScrollBar.vertical:ScrollBar{}
   }
  }
 }
 Dialog {id:downloadDialog;property url chosenFolder:"";title:"Download a file";modal:true;anchors.centerIn:parent;width:Math.min(root.width-32,470);standardButtons:Dialog.Ok|Dialog.Cancel
  onOpened:chosenFolder=""
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   TextField {id:downloadUrl;Layout.fillWidth:true;placeholderText:"https:// download address";selectByMouse:true;maximumLength:4096}
   TextField {id:downloadName;Layout.fillWidth:true;placeholderText:"Save as (including extension)";selectByMouse:true;maximumLength:200}
   Label {Layout.fillWidth:true;text:downloadDialog.chosenFolder.toString()?decodeURIComponent(downloadDialog.chosenFolder.toString().replace(/^file:\/\//,"")):Browser.downloadsPath();wrapMode:Text.WrapAnywhere;color:SettingsService.colors.muted}
   Tool {text:"Choose Save Location";onClicked:{downloadSaveFolder.currentFolder=Browser.lastDownloadFolder;downloadSaveFolder.open()}}
  }
  onAccepted:{Browser.download(downloadUrl.text.trim(),downloadName.text.trim(),chosenFolder);downloadUrl.clear();downloadName.clear()}
 }
 FolderDialog {id:downloadSaveFolder;title:"Choose where to save this download";onAccepted:downloadDialog.chosenFolder=selectedFolder}
 FolderDialog {id:extractPicker;property string archivePath;title:"Extract to an existing folder"
  onAccepted:Browser.archiveAction("extract",archivePath,decodeURIComponent(selectedFolder.toString().replace(/^file:\/\//,"")),false,"")}
 FolderDialog {id:downloadFolder;title:"Choose the default Downloads folder";onAccepted:Browser.setDownloadsPath(selectedFolder)}
 FolderDialog {id:movePicker;property var paths:[];title:"Move selected items to";onAccepted:Browser.transferTo(paths,selectedFolder,true)}
 Dialog {id:deepSearchDialog;title:"Find in this folder and subfolders";modal:true;anchors.centerIn:parent;width:Math.min(root.width-32,430);standardButtons:Dialog.Ok|Dialog.Cancel
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   TextField {id:deepQuery;Layout.fillWidth:true;placeholderText:"Name, extension or text";selectByMouse:true;maximumLength:300}
   SovelCombo {id:deepKind;model:["All types","Folders","Images","Video","Audio","Text","Archives"]}
   RowLayout {Label {text:"Modified in last days (0 for any)"} SpinBox {id:daysOld;from:0;to:3650}}
   RowLayout {Label {text:"Minimum size (MiB)"} SpinBox {id:minimumSize;from:0;to:1000000}}
   CheckBox {id:deepContents;text:"Search the first 64 KiB of text files"}
  }
  onAccepted:{search.clear();Browser.searchFolder(deepQuery.text,["all","folder","image/","video/","audio/","text/","archive"][deepKind.currentIndex],daysOld.value,minimumSize.value,deepContents.checked)}
 }
 Dialog {id:archiveDialog;title:"Archive contents";modal:true;anchors.centerIn:parent;width:Math.min(root.width-32,600);height:Math.min(root.height-32,480);standardButtons:Dialog.Close
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   Label {Layout.fillWidth:true;text:Browser.busy?Browser.operationProgress||"Reading archive…":Browser.status;wrapMode:Text.WordWrap}
   ListView {Layout.fillWidth:true;Layout.fillHeight:true;model:Browser.archiveEntries;clip:true
    delegate:Label {required property var modelData;width:ListView.view.width;height:26;text:modelData.name;verticalAlignment:Text.AlignVCenter}
    ScrollBar.vertical:ScrollBar{}
   }
   TextField {id:archivePassword;Layout.fillWidth:true;placeholderText:"Password, if required";echoMode:TextInput.Password}
   RowLayout {
    Tool {text:"Preview";enabled:!Browser.busy&&!!root.selectedEntry;onClicked:Browser.archiveAction("preview",root.selected[0],"",false,archivePassword.text)}
    Tool {text:"Test";enabled:!Browser.busy&&!!root.selectedEntry;onClicked:Browser.archiveAction("test",root.selected[0],"",false,archivePassword.text)}
    Tool {text:"Extract to New Folder";enabled:root.inFolder&&!Browser.busy&&!!root.selectedEntry;onClicked:Browser.archiveAction("extract",root.selected[0],Browser.path,true,archivePassword.text)}
   }
  }
  onClosed:archivePassword.clear()
 }
 Dialog {id:compressDialog;title:"Create archive";modal:true;anchors.centerIn:parent;width:390;standardButtons:Dialog.Ok|Dialog.Cancel
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   TextField {id:archiveName;Layout.fillWidth:true;placeholderText:"Archive name"}
   SovelCombo {id:archiveFormat;model:["zip","7z","tar","tar.gz","tar.xz","gz"]}
   Label {text:"Compression level "+archiveLevel.value.toFixed(0)}
   Slider {id:archiveLevel;from:0;to:9;stepSize:1;value:6;Layout.fillWidth:true;enabled:archiveFormat.currentText!=="tar"&&archiveFormat.currentText!=="7z"}
  }
  onAccepted:{const name=archiveName.text.trim();if(name&&!name.includes("/")&&name!=="."&&name!=="..")Browser.compress(root.selected,Browser.path+"/"+name+"."+archiveFormat.currentText,archiveLevel.value)}
 }
 Dialog {id:driveDialog;property bool openingDrive:false;title:root.activeVolume.name||"Drive details";modal:true;anchors.centerIn:parent;width:Math.min(root.width-24,500);standardButtons:Dialog.Close
  onOpened:openingDrive=false
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   Label {Layout.fillWidth:true;text:root.activeVolume.mount||"Detected · Not mounted";wrapMode:Text.WrapAnywhere}
   Label {Layout.fillWidth:true;text:[root.activeVolume.device,root.activeVolume.filesystem,root.activeVolume.connection,root.activeVolume.capacity?(root.activeVolume.capacity/1073741824).toFixed(1)+" GiB":"",root.activeVolume.available!==null&&root.activeVolume.available!==undefined?(root.activeVolume.available/1073741824).toFixed(1)+" GiB available":"",root.activeVolume.readonly?"Read only":""].filter(Boolean).join(" · ");wrapMode:Text.WordWrap}
   Label {Layout.fillWidth:true;visible:!root.activeVolume.mount;text:root.activeVolume.mountable?"Mount this drive to browse its files.":"This partition does not have a filesystem Files can mount.";wrapMode:Text.WordWrap}
   Label {Layout.fillWidth:true;visible:!!root.activeVolume.protected;text:"System volume. Drive changes are disabled.";wrapMode:Text.WordWrap}
   Label {Layout.fillWidth:true;text:Browser.status;wrapMode:Text.WordWrap;visible:!!text}
   RowLayout {
    Tool {text:root.activeVolume.mount?"Open":"Mount Drive";enabled:!Browser.busy&&(!!root.activeVolume.mount||!!root.activeVolume.mountable&&!root.activeVolume.protected);onClicked:{driveDialog.openingDrive=!root.activeVolume.mount;Browser.volumeAction("open",root.activeVolume.id,"");if(root.activeVolume.mount)driveDialog.close()}}
    Tool {text:"Unmount";enabled:!Browser.busy&&!!root.activeVolume.mount&&!root.activeVolume.protected;onClicked:Browser.volumeAction("unmount",root.activeVolume.id,"")}
    Tool {text:"Safely Eject";enabled:!Browser.busy&&!!root.activeVolume.removable&&!root.activeVolume.protected;onClicked:Browser.volumeAction("eject",root.activeVolume.id,"")}
   }
   RowLayout {
    Tool {text:"Check Filesystem";enabled:!Browser.busy&&!root.activeVolume.mount&&!!root.activeVolume.mountable&&!root.activeVolume.protected;onClicked:Browser.volumeAction("check",root.activeVolume.id,"")}
    TextField {id:volumeLabel;Layout.fillWidth:true;placeholderText:"New label";enabled:!root.activeVolume.protected&&!root.activeVolume.readonly}
    Tool {text:"Rename";enabled:!Browser.busy&&volumeLabel.enabled&&!!volumeLabel.text.trim();onClicked:Browser.volumeAction("label",root.activeVolume.id,volumeLabel.text)}
   }
   Tool {text:"Format Volume…";visible:!!root.activeVolume.removable&&!root.activeVolume.protected;enabled:!Browser.busy&&!root.activeVolume.mount&&!root.activeVolume.readonly&&!!root.activeVolume.identity
    onClicked:{formatVolume.target=Object.assign({},root.activeVolume);formatConfirm.clear();formatVolume.open()}}
  }
  Connections {target:Browser
   function onPathChanged(){if(driveDialog.visible&&driveDialog.openingDrive)driveDialog.close()}
   function onChanged(){
    if(!driveDialog.visible)return
    const item=Browser.volumes.find(v=>v.id===root.activeVolume.id)
    if(item)root.activeVolume=item
    else if(!Browser.volumeError)root.activeVolume=({name:"Drive disconnected"})
    if(!Browser.busy)driveDialog.openingDrive=false
   }
  }
 }
 Dialog {id:formatVolume;property var target:({});title:"Erase this removable volume?";modal:true;anchors.centerIn:parent;width:Math.min(root.width-28,490);standardButtons:Dialog.Cancel
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   Label {Layout.fillWidth:true;text:"Formatting "+(formatVolume.target.name||"this volume")+" permanently erases its contents. Double-check the device shown below.";wrapMode:Text.WordWrap}
   Label {text:formatVolume.target.device||"";font.bold:true}
   SovelCombo {id:formatFilesystem;model:["exfat","vfat","ext4"];Accessible.name:"New filesystem"}
   TextField {id:formatConfirm;Layout.fillWidth:true;placeholderText:"Type the device path to confirm"}
   Tool {text:"Erase and Format";enabled:formatConfirm.text===formatVolume.target.device&&!Browser.busy
    onClicked:{Browser.formatVolume(formatVolume.target.id,formatFilesystem.currentText,formatConfirm.text,formatVolume.target.identity);formatVolume.close()}}
  }
 }
 Dialog {id:preferencesDialog;title:"Files Preferences";modal:true;anchors.centerIn:parent;width:410;standardButtons:Dialog.Close
  background:ShellGlass {radius:16;tint:.97}
  contentItem:ColumnLayout {
   Label {text:"File views";font.bold:true}
   CheckBox {text:"Show hidden files";checked:Browser.showHidden;onToggled:Browser.showHidden=checked}
   SovelCombo {model:["list","compact","grid","gallery"];currentIndex:model.indexOf(Browser.viewMode);onActivated:Browser.viewMode=currentText}
   Label {text:"Downloads";font.bold:true}
   Label {Layout.fillWidth:true;text:Browser.downloadsPath();wrapMode:Text.WrapAnywhere}
   Tool {text:"Open Downloads";onClicked:{Browser.navigate(Browser.downloadsPath());preferencesDialog.close()}}
   Tool {text:"Change Downloads Folder…";enabled:!Browser.busy;onClicked:downloadFolder.open()}
   CheckBox {text:"Organize Sovel downloads by file type";checked:Browser.organizeDownloads;onToggled:Browser.organizeDownloads=checked}
   CheckBox {text:"Show download notifications";checked:Browser.notifyDownloads;onToggled:Browser.notifyDownloads=checked}
   Label {Layout.fillWidth:true;text:"Downloads from other apps appear here as files. Use the originating app to control its active downloads.";wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
   Label {text:"External Drives";font.bold:true}
   CheckBox {text:"Automatically mount USB drives while Files is open";checked:Browser.autoMount;onToggled:Browser.autoMount=checked}
  }
 }
 Dialog {id:nameDialog;property bool renaming:false;title:renaming?"Rename":"New Folder";anchors.centerIn:parent;modal:true;width:340;standardButtons:Dialog.Ok|Dialog.Cancel
  background:Rectangle {radius:15;color:"#F02A1B37";border.color:"#9C70BB"}
  TextField {id:nameField;width:parent.width;color:"#F7EBFF";selectByMouse:true;placeholderText:"Name";onAccepted:nameDialog.accept()}
  onOpened:{nameField.forceActiveFocus();nameField.selectAll()}
  onAccepted:{if(renaming)Browser.rename(root.selected[0],nameField.text);else Browser.createFolder(nameField.text)}
 }
 Dialog {id:trashDialog;title:"Move to Trash?";anchors.centerIn:parent;modal:true;width:350;standardButtons:Dialog.Ok|Dialog.Cancel
  background:Rectangle {radius:15;color:"#F02A1B37";border.color:"#9C70BB"}
  Label {width:parent.width;text:"Move "+root.selected.length+" selected items to Trash? You can recover them later.";wrapMode:Text.WordWrap}
  onAccepted:Browser.trash(root.selected)
 }
 Shortcut {sequence:"Ctrl+L";onActivated:{root.editingPath=true;pathField.forceActiveFocus();pathField.selectAll()}}
 Shortcut {sequence:"Ctrl+T";onActivated:Browser.newTab()}
 Shortcut {sequence:"Ctrl+W";enabled:Browser.tabs.length>1;onActivated:Browser.closeTab(Browser.activeTab)}
 Shortcut {sequence:"Space";enabled:root.activeFocus&&!!root.selectedEntry;onActivated:{if(root.selectedEntry.archive)root.open();else {root.previewDetails=Browser.inspect(root.selected[0]);previewDialog.open()}}}
 Shortcut {sequence:"Ctrl+F";onActivated:search.forceActiveFocus()}
 Shortcut {sequence:"Ctrl+H";onActivated:Browser.showHidden=!Browser.showHidden}
 Shortcut {sequence:"Alt+Left";onActivated:Browser.back()}
 Shortcut {sequence:"Alt+Right";onActivated:Browser.forward()}
 Shortcut {sequence:"Alt+Up";onActivated:Browser.up()}
 Shortcut {sequence:"F5";onActivated:Browser.refresh()}
 Shortcut {sequence:"Ctrl+A";enabled:root.activeFocus;onActivated:root.selected=root.rows.map(e=>e.path)}
 Shortcut {sequence:"Ctrl+C";enabled:root.activeFocus;onActivated:Browser.copy(root.selected,false)}
 Shortcut {sequence:"Ctrl+X";enabled:root.activeFocus;onActivated:Browser.copy(root.selected,true)}
 Shortcut {sequence:"Ctrl+V";enabled:root.activeFocus;onActivated:Browser.paste()}
 Shortcut {sequence:"F2";onActivated:root.rename()}
 Shortcut {sequence:"Delete";enabled:root.activeFocus;onActivated:root.trash()}
 Shortcut {sequence:"Return";enabled:root.activeFocus;onActivated:root.open()}
 Shortcut {sequence:"Ctrl+Shift+N";onActivated:{nameDialog.renaming=false;nameField.clear();nameDialog.open()}}
}
