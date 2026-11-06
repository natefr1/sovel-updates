import QtQuick
import QtQuick.Controls
ToolTip {
 id:root
 popupType:Popup.Item
 padding:9
 background:ShellGlass {radius:10;tint:0.7}
 contentItem:Text {text:root.text;color:"#F8EEFF";font.pixelSize:12;wrapMode:Text.Wrap}
 enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:System.reducedMotion?0:100}}
 exit:Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:System.reducedMotion?0:70}}
}
