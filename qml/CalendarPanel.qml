import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
 id:root
 property date today:new Date()
 property date month:new Date(today.getFullYear(),today.getMonth(),1,12)
 property int firstDay:Qt.locale().firstDayOfWeek%7
 property bool compact:typeof sovelWindow!=="undefined"&&sovelWindow.height<500
 readonly property var days:{let first=new Date(month.getFullYear(),month.getMonth(),1,12);let offset=(first.getDay()-firstDay+7)%7;let result=[];for(let i=0;i<42;i++)result.push(new Date(month.getFullYear(),month.getMonth(),1-offset+i,12));return result}
 spacing:12
 function shift(delta){month=new Date(month.getFullYear(),month.getMonth()+delta,1,12)}
 RowLayout {
  Text {Layout.fillWidth:true;text:Qt.formatDateTime(root.month,"MMMM yyyy");color:"#F7EDFF";font.pixelSize:19;font.bold:true}
  SovelButton {text:"‹";implicitWidth:32;onClicked:root.shift(-1);Accessible.name:"Previous month"}
  SovelButton {text:"Today";onClicked:root.month=new Date(root.today.getFullYear(),root.today.getMonth(),1,12)}
  SovelButton {text:"›";implicitWidth:32;onClicked:root.shift(1);Accessible.name:"Next month"}
 }
 GridLayout {
  columns:7;rowSpacing:5;columnSpacing:5
  Repeater {model:7;Text {required property int index;Layout.preferredWidth:36;Layout.preferredHeight:24;horizontalAlignment:Text.AlignHCenter;text:Qt.locale().standaloneDayName(((root.firstDay+index)%7)||7,Locale.ShortFormat);color:"#CDB8DF";font.pixelSize:10}}
  Repeater {model:root.days
   Rectangle {required property var modelData;Layout.preferredWidth:36;Layout.preferredHeight:root.compact?24:32;radius:10
    readonly property bool isToday:modelData.toDateString()===root.today.toDateString()
    color:isToday?"#9C63CA":"transparent"
    Text {anchors.centerIn:parent;text:modelData.getDate();color:parent.isToday?"#FFFFFF":modelData.getMonth()===root.month.getMonth()?"#F7EDFF":"#8E789F";font.pixelSize:13;font.bold:parent.isToday}
   }
  }
 }
 Text {Layout.fillWidth:true;text:Qt.formatDateTime(root.today,"dddd, MMMM d yyyy");color:"#CDB8DF";font.pixelSize:12;horizontalAlignment:Text.AlignHCenter}
 Timer {interval:30000;running:true;repeat:true;onTriggered:root.today=new Date()}
}
