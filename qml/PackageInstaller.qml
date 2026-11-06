import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
 id:root;width:880;height:680
 Rectangle {anchors.fill:parent;color:"#17111F"}
 Connections {target:sovelWindow;function onClosing(event){if(Packages.busy)event.accepted=false}}
 Glass {anchors.fill:parent;radius:0;tint:0.85;sceneSize:Qt.size(root.width,root.height)}
 ColumnLayout {
  anchors.fill:parent;anchors.margins:30;spacing:18
  RowLayout {spacing:18
   Image {source:"image://apps/application-x-deb";Layout.preferredWidth:64;Layout.preferredHeight:64}
   ColumnLayout {
    Text {text:Packages.details.Package||"Package Installer";color:"#F9F1FF";font.pixelSize:26;font.weight:Font.DemiBold;Layout.fillWidth:true;wrapMode:Text.WordWrap}
    Text {text:(Packages.details.Version||"")+(Packages.details.Architecture?" · "+Packages.details.Architecture:"");color:"#C6B2D7";font.pixelSize:13}
   }
  }
  Text {text:Packages.status;color:"#E5CFF8";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:14}
  Text {text:Packages.error;color:"#FFB5C6";visible:text.length>0;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:13}
  ProgressBar {Layout.fillWidth:true;visible:Packages.busy;from:0;to:100;value:Math.max(0,Packages.progress);indeterminate:Packages.progress<0}
  ScrollView {
   id:scroll;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
   ColumnLayout {width:scroll.availableWidth;spacing:18
    Text {text:Packages.details.Description||"Open a .deb file from Downloads to review and install it.";color:"#D2BFE0";textFormat:Text.PlainText;Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:14;lineHeight:1.3}
    Text {text:Packages.details.filename?"File: "+Packages.details.filename:"";color:"#B49CC7";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:12}
    Text {text:"Installation changes";visible:Packages.changes.length>0;color:"#F5E8FF";font.pixelSize:18;font.weight:Font.DemiBold}
    Repeater {model:Packages.changes
     Rectangle {required property var modelData;Layout.fillWidth:true;implicitHeight:64;radius:11;color:"#302B1E3C";border.color:"#345A3D70"
      Column {anchors.fill:parent;anchors.margins:12;spacing:4
       Text {text:modelData.action+" "+modelData.name;color:modelData.action==="Remove"?"#FFC1CA":"#F3E6FF";font.pixelSize:13;textFormat:Text.PlainText}
       Text {text:modelData.version;color:"#BFA4D1";font.pixelSize:11}
      }
     }
    }
   }
  }
  Text {text:"Only install packages from publishers you trust. Downloaded packages can make system changes and run installation scripts.";visible:!Packages.installed;color:"#BDA6CE";Layout.fillWidth:true;wrapMode:Text.WordWrap;font.pixelSize:12}
  RowLayout {Layout.fillWidth:true
   SovelButton {text:"Files";onClicked:System.launch("files")}
   Item {Layout.fillWidth:true}
   SovelButton {text:"Cancel";visible:Packages.busy;enabled:Packages.canCancel;onClicked:Packages.cancel()}
   SovelButton {text:Packages.installed?"App Library":Packages.reviewed?"Install":"Review Installation";primary:true;enabled:!Packages.busy&&(Packages.installed||!!Packages.details.Package);onClicked:Packages.installed?System.launch("library"):Packages.reviewed?Packages.install():Packages.review()}
  }
 }
}
