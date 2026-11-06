import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
 id:root;spacing:12
 property var keys:String(SettingsService.preferences["bar.order"]||"network,bluetooth,sound,battery,notifications,updates,jared,search,control,calendar").split(",").concat(["downloads"]).filter((key,index,all)=>all.indexOf(key)===index)
 function move(index,step){let values=keys.slice();const destination=index+step;if(destination<0||destination>=values.length)return;const value=values[index];values[index]=values[destination];values[destination]=value;SettingsService.setPreference("bar.order",values.join(","));keys=values}
 SettingsLabel {text:"Choose the left-to-right order of status icons. Visibility switches are on the Menu Bar page.";color:SettingsService.colors.muted}
 Repeater {model:root.keys
  RowLayout {required property string modelData;required property int index;Layout.fillWidth:true
   SettingsLabel {text:({network:"Wi-Fi",bluetooth:"Bluetooth",sound:"Sound",battery:"Battery",notifications:"Notifications",updates:"Updates",jared:"JARED",search:"App search",control:"Control Center",calendar:"Calendar",downloads:"Downloads"})[modelData]||modelData}
   SovelButton {text:"↑";enabled:index>0;onClicked:root.move(index,-1);Accessible.name:"Move "+modelData+" left"}
   SovelButton {text:"↓";enabled:index<root.keys.length-1;onClicked:root.move(index,1);Accessible.name:"Move "+modelData+" right"}
  }
 }
}
