import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
 id: root; width: 1120; height: 740
 property string collection: "Discover"
 property bool showingDetails: false
 onShowingDetailsChanged: arrival.restart()
 NumberAnimation {id:arrival;target:root.showingDetails?detailScroll:browseScroll;property:"opacity";from:0.35;to:1;duration:System.reducedMotion?0:160;easing.type:Easing.OutCubic}
 property var app: StoreService.details
 readonly property bool installed: StoreService.installed.indexOf(app.id || "")>=0
 readonly property var lastJob:(StoreService.jobs||[]).find(job=>job.app===(root.app.id||""))||null
 readonly property var activeJob:lastJob&&["queued","running"].includes(lastJob.state)?lastJob:null
 property var sections: ["Discover","Create","Work","Play","Develop","Utilities","All Apps","Installed","Updates","Downloads"]
 function browse(name){collection=name;showingDetails=false;search.clear();if(name==="Updates")StoreService.checkAppUpdates();StoreService.browse(name)}
 function select(entry){showingDetails=true;StoreService.select(entry.id)}
 Component.onCompleted: {StoreService.setWatching(true);StoreService.refreshInstalled();StoreService.browse("Discover")}
 Component.onDestruction:StoreService.setWatching(false)
 Shortcut {sequence:"Ctrl+F";onActivated:{search.forceActiveFocus();search.selectAll()}}
 Shortcut {sequence:"Alt+Left";enabled:root.showingDetails;onActivated:root.showingDetails=false}
 Rectangle {anchors.fill:parent;color:"#17111F"}
 Glass {anchors.fill:parent;radius:0;tint:0.88;sceneSize:Qt.size(root.width,root.height)}
 component Label: Text {color:"#FAF5FF";textFormat:Text.PlainText;wrapMode:Text.WordWrap}
 component Subtext: Label {color:"#BCAECF";font.pixelSize:12}
 component AppIcon: Rectangle {
  property string url:"";property string name:""
  radius:width*0.22;color:"#32233F";border.color:"#38DDC4FF"
  Label {anchors.centerIn:parent;text:parent.name.slice(0,1);font.pixelSize:parent.width*0.42;visible:icon.status!==Image.Ready}
  Image {id:icon;anchors.fill:parent;anchors.margins:2;source:parent.url;asynchronous:true;fillMode:Image.PreserveAspectFit;sourceSize:Qt.size(160,160)}
 }
 Rectangle {
  id:sidebar;width:root.width<850?176:210;height:parent.height;color:"#95231932"
  ColumnLayout {
   anchors.fill:parent;anchors.margins:16;spacing:8
   RowLayout {spacing:9
    Image {source:"qrc:/assets/icons/store.svg";Layout.preferredWidth:28;Layout.preferredHeight:28}
    Label {text:"Store";font.pixelSize:23;font.weight:Font.DemiBold}
   }
   TextField {
    id:search;objectName:"store-search";Layout.fillWidth:true;color:"#FAF3FF";placeholderText:"Search apps";placeholderTextColor:"#BCAECF"
    background:Rectangle {radius:10;color:"#73130D1B";border.color:search.activeFocus?"#C095FF":"#465D456B"}
    onAccepted:{if(text.trim().length){root.showingDetails=false;StoreService.search(text)}}
    Accessible.name:"Search the app catalog"
   }
   SovelButton {text:"Search";Layout.fillWidth:true;enabled:!StoreService.busy&&search.text.trim().length>0;onClicked:search.accepted()}
   Repeater {
    model:root.sections
    Button {
     required property string modelData
     Layout.fillWidth:true;implicitHeight:37;text:modelData
     onClicked:root.browse(modelData)
     background:Rectangle {radius:10;color:!root.showingDetails&&root.collection===parent.modelData?"#8953BD":parent.hovered?"#24FFFFFF":"transparent"}
     contentItem:Label {text:parent.text;leftPadding:10;verticalAlignment:Text.AlignVCenter;font.pixelSize:13}
    }
   }
   Item {Layout.fillHeight:true}
   Subtext {text:"PERSONALIZE";font.pixelSize:10;font.letterSpacing:1.5}
   SovelButton {text:"Waypaper";Layout.fillWidth:true;onClicked:System.launch("wallpapers")}
   Subtext {text:"Apps from Flathub and reviewed official sources. Downloads continue when Store is closed.";Layout.fillWidth:true}
  }
 }
 ColumnLayout {
  anchors.left:sidebar.right;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.margins:24;spacing:14
  RowLayout {
   Layout.fillWidth:true
   SovelButton {text:"‹";visible:root.showingDetails;implicitWidth:36;onClicked:root.showingDetails=false;Accessible.name:"Back to apps"}
   Label {Layout.fillWidth:true;text:root.showingDetails?"App Details":search.text.trim()?"Results for “"+search.text.trim()+"”":root.collection;font.pixelSize:26;font.weight:Font.DemiBold}
   BusyIndicator {running:StoreService.busy||StoreService.detailsBusy||StoreService.status==="Checking app updates…";visible:running;Layout.preferredWidth:26;Layout.preferredHeight:26}
   SovelButton {visible:StoreService.busy;text:"Cancel";onClicked:StoreService.cancel()}
  }
  Subtext {text:StoreService.status;Layout.fillWidth:true}
  SovelButton {visible:root.collection==="Updates"&&!root.showingDetails;text:"Check app updates";onClicked:StoreService.checkAppUpdates()}
  ScrollView {id:downloadScroll;visible:root.collection==="Downloads"&&!root.showingDetails;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   ColumnLayout {width:downloadScroll.availableWidth;spacing:12
    Repeater {model:StoreService.jobs
     Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:148;radius:16;color:"#382F243D";border.color:"#38DAC4F0"
      ColumnLayout {anchors.fill:parent;anchors.margins:16;spacing:8
       Label {text:modelData.app;font.bold:true;Layout.fillWidth:true}
       Subtext {text:modelData.status;Layout.fillWidth:true}
       ProgressBar {Layout.fillWidth:true;from:0;to:100;value:Math.max(0,modelData.progress);indeterminate:modelData.progress<0&&(modelData.state==="running"||modelData.state==="queued")}
       RowLayout {Subtext {text:modelData.state;Layout.fillWidth:true}
        SovelButton {text:"Cancel";visible:(modelData.state==="running"||modelData.state==="queued")&&modelData.app!=="org.kde.kdeconnect";onClicked:StoreService.cancelJob(modelData.job)}
        SovelButton {text:"Review & retry";visible:modelData.state==="failed"||modelData.state==="cancelled";onClicked:StoreService.retryJob(modelData.job)}
       }
      }
     }
    }
    Subtext {visible:!StoreService.jobs.length;text:"No downloads yet. Choose an app and select Install."}
   }
  }
  ScrollView {
   id:browseScroll;visible:!root.showingDetails&&root.collection!=="Downloads";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   ColumnLayout {
    width:browseScroll.availableWidth;spacing:22
    Rectangle {
     visible:root.collection==="Discover"&&!search.text.trim();Layout.fillWidth:true;implicitHeight:182;radius:22
     gradient:Gradient {orientation:Gradient.Horizontal;GradientStop {position:0;color:"#653A91"}GradientStop {position:1;color:"#282A61"}}
     Column {anchors.fill:parent;anchors.margins:25;spacing:11
      Label {text:"MADE FOR YOUR EVERYDAY";font.pixelSize:10;font.letterSpacing:1.7;color:"#D5B8F0"}
      Label {width:parent.width;text:"A little inspiration.\nYour next favorite app.";font.pixelSize:27;font.weight:Font.DemiBold}
      Subtext {text:"Browse, create, play. Make Sovel your own.";color:"#E1CFF2"}
     }
    }
    GridLayout {
     id:grid;columns:width>890?3:width>520?2:1;columnSpacing:16;rowSpacing:16;Layout.fillWidth:true
     Repeater {
      model:StoreService.results
      Rectangle {
       required property var modelData
       Layout.fillWidth:true;Layout.preferredWidth:280;implicitHeight:310;radius:19;color:cardHover.hovered?"#664B365F":"#382F243D";border.color:"#38DAC4F0"
       HoverHandler {id:cardHover}
       ColumnLayout {
        anchors.fill:parent;anchors.margins:16;spacing:12
        RowLayout {
         Layout.fillWidth:true;spacing:11
         AppIcon {url:modelData.icon||"";name:modelData.name;Layout.preferredWidth:45;Layout.preferredHeight:45}
         ColumnLayout {Layout.fillWidth:true;spacing:3
          Label {text:modelData.name;font.pixelSize:14;font.weight:Font.DemiBold;Layout.fillWidth:true;maximumLineCount:1;elide:Text.ElideRight}
          Subtext {text:modelData.description;Layout.fillWidth:true;maximumLineCount:2;elide:Text.ElideRight}
         }
         SovelButton {text:"View";implicitWidth:58;onClicked:root.select(modelData)}
        }
        Rectangle {
         Layout.fillWidth:true;Layout.fillHeight:true;radius:12;clip:true;color:"#402332"
         Image {id:preview;anchors.fill:parent;asynchronous:true;fillMode:Image.PreserveAspectFit;source:modelData.screenshots&&modelData.screenshots.length?modelData.screenshots[0]:"";sourceSize.width:620}
         Column {anchors.centerIn:parent;width:parent.width-30;spacing:8;visible:preview.status!==Image.Ready
          Label {width:parent.width;text:modelData.name;horizontalAlignment:Text.AlignHCenter;font.pixelSize:23;font.weight:Font.DemiBold}
          Subtext {width:parent.width;text:"Explore this app";horizontalAlignment:Text.AlignHCenter}
         }
         MouseArea {anchors.fill:parent;onClicked:root.select(modelData);cursorShape:Qt.PointingHandCursor}
        }
        Subtext {text:StoreService.installed.indexOf(modelData.id)>=0?(StoreService.updatable.indexOf(modelData.id)>=0?"Update available":"Installed"):(modelData.sourceLabel||"Flathub");font.pixelSize:10}
        SovelButton {text:"Update";visible:root.collection==="Updates";onClicked:StoreService.updateApp(modelData.id)}
       }
      }
     }
    }
    Subtext {Layout.fillWidth:true;visible:!StoreService.results.length&&!StoreService.busy;text:root.collection==="Updates"?StoreService.status:"No apps here yet. Search by name to explore the full catalog."}
   }
  }
  ScrollView {
   id:detailScroll;visible:root.showingDetails;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   ColumnLayout {
    width:detailScroll.availableWidth;spacing:23
    Rectangle {
     Layout.fillWidth:true;implicitHeight:196;radius:21
     gradient:Gradient {orientation:Gradient.Horizontal;GradientStop {position:0;color:"#6D418B"}GradientStop {position:1;color:"#29233C"}}
     RowLayout {
      anchors.fill:parent;anchors.margins:24;spacing:22
      AppIcon {url:root.app.icon||"";name:root.app.name||"";Layout.preferredWidth:100;Layout.preferredHeight:100}
      ColumnLayout {Layout.fillWidth:true;spacing:8
       Label {text:root.app.name||"App";Layout.fillWidth:true;font.pixelSize:27;font.weight:Font.DemiBold}
       Label {text:root.app.description||"";Layout.fillWidth:true;font.pixelSize:13}
       Subtext {text:(root.app.developer||"Flathub app")+(root.app.verified?" · Verified publisher":"");color:"#D4BFE4"}
       SovelButton {objectName:"store-install";text:root.activeJob?"Installing…":root.installed?"Open":"Install";primary:true;enabled:!StoreService.busy&&!root.activeJob;onClicked:root.installed?StoreService.open(root.app.id):StoreService.review(root.app.id)}
       ProgressBar {visible:!!root.activeJob;Layout.fillWidth:true;from:0;to:100;value:root.activeJob?Math.max(0,root.activeJob.progress):0;indeterminate:!!root.activeJob&&root.activeJob.progress<0}
       Subtext {visible:!!root.activeJob;text:root.activeJob?root.activeJob.status:"";Layout.fillWidth:true}
       SovelButton {visible:!!root.activeJob&&root.app.sourceKind!=="apt";text:"Cancel installation";onClicked:StoreService.cancelJob(root.activeJob.job)}
       SovelButton {visible:!!root.lastJob&&["failed","cancelled"].includes(root.lastJob.state);text:"Review & retry";onClicked:StoreService.retryJob(root.lastJob.job)}
       SovelButton {text:"Update";visible:StoreService.updatable.indexOf(root.app.id||"")>=0;onClicked:StoreService.updateApp(root.app.id)}
      }
     }
    }
    RowLayout {Layout.fillWidth:true;spacing:20
     ColumnLayout {Layout.fillWidth:true;Subtext {text:"VERSION";font.pixelSize:10}Label {text:root.app.version||"See latest release";font.pixelSize:16}}
     ColumnLayout {Layout.fillWidth:true;Subtext {text:"LICENSE";font.pixelSize:10}Label {text:root.app.license||"See app information";font.pixelSize:14}}
     ColumnLayout {Layout.fillWidth:true;Subtext {text:"SOURCE";font.pixelSize:10}Label {text:root.app.sourceLabel||"Flathub";font.pixelSize:16}}
    }
    Label {text:"Preview";font.pixelSize:19;font.weight:Font.DemiBold}
    ListView {
     id:screenshots;Layout.fillWidth:true;Layout.preferredHeight:280;orientation:ListView.Horizontal;spacing:14;clip:true;model:root.app.screenshots||[];boundsBehavior:Flickable.StopAtBounds
     delegate:Rectangle {required property string modelData;width:Math.min(480,screenshots.width-12);height:260;radius:14;color:"#241B30"
      Image {anchors.fill:parent;anchors.margins:2;asynchronous:true;fillMode:Image.PreserveAspectFit;source:modelData;sourceSize.width:960}
     }
     ScrollBar.horizontal:ScrollBar {}
    }
    Subtext {visible:!screenshots.count;text:"No preview images supplied by this publisher."}
    Label {text:"About this app";font.pixelSize:19;font.weight:Font.DemiBold}
    Text {text:root.app.longDescription||root.app.description||"Select an app to see its information.";textFormat:Text.StyledText;color:"#D4C5DF";font.pixelSize:14;lineHeight:1.4;wrapMode:Text.WordWrap;Layout.fillWidth:true}
    Subtext {text:root.app.id||"";Layout.fillWidth:true}
    Subtext {text:"Source: "+(root.app.sourceLabel||"Flathub")+". Some apps require accounts, extra downloads or paid subscriptions.";Layout.fillWidth:true}
   }
  }
 }
 Dialog {
  id:permissions;anchors.centerIn:parent;width:Math.min(660,root.width-50);height:Math.min(570,root.height-50)
  modal:true;title:"Install "+(root.app.name||StoreService.selectedId)
  background:Rectangle {radius:20;color:"#F02B1D3C";border.color:"#B68AD7"}
  footer:DialogButtonBox {Button {text:"Cancel";DialogButtonBox.buttonRole:DialogButtonBox.RejectRole}Button {text:"Install";DialogButtonBox.buttonRole:DialogButtonBox.AcceptRole}}
  onAccepted:StoreService.installSelected()
  contentItem:ColumnLayout {
   Subtext {text:"Review the source and app access before continuing.";Layout.fillWidth:true}
   ScrollView {Layout.fillWidth:true;Layout.fillHeight:true;TextArea {text:StoreService.permissions;readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;color:"#EEE3FA";font.family:"monospace";font.pixelSize:11}}
  }
 }
 Connections {target:StoreService;function onReviewReady(){permissions.open()}function onInstalledChanged(){if(!root.showingDetails&&root.collection==="Installed")StoreService.browse("Installed")}function onJobsChanged(){if(!root.showingDetails&&root.collection==="Updates")StoreService.browse("Updates")}}
}
