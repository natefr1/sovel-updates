import QtQuick
import QtQuick.Controls
// The one dropdown surface every top-bar menu uses: Wi-Fi, Bluetooth, Sound,
// Calendar, Notifications, JARED, Downloads and Control Center.
// Escape and a click outside close it; opening another menu closes this one
// (System.claimMenu). It never changes the output layout.
Popup {
 id:root;popupType:Popup.Item;focus:true;padding:16;margins:8
 closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside|Popup.CloseOnReleaseOutside
 readonly property bool still:typeof System==="undefined"||!System||System.reducedMotion
 background:ShellGlass {width:root.width;height:root.height;radius:16;tint:0.86}
 onAboutToShow:{System.claimMenu(root);if(typeof Chrome!=="undefined")Chrome.registerPopup(root,true)}
 Component.onDestruction:if(typeof Chrome!=="undefined")Chrome.registerPopup(root,false)
 onClosed:if(typeof Chrome!=="undefined")Chrome.registerPopup(root,false)
 Connections {target:sovelWindow;function onActiveChanged(){if(!sovelWindow.active&&root.visible)root.close()}}
 Connections {target:Qt.application;function onStateChanged(){if(Qt.application.state!==Qt.ApplicationActive)root.close()}}
 // Fade and a slight scale: the popup's size never animates, so content does not reflow.
 enter:Transition {ParallelAnimation {
  NumberAnimation {property:"opacity";from:0;to:1;duration:root.still?0:150;easing.type:Easing.OutCubic}
  NumberAnimation {property:"scale";from:root.still?1:0.97;to:1;duration:root.still?0:150;easing.type:Easing.OutCubic}
 }}
 exit:Transition {ParallelAnimation {
  NumberAnimation {property:"opacity";from:1;to:0;duration:root.still?0:120;easing.type:Easing.InCubic}
  NumberAnimation {property:"scale";from:1;to:root.still?1:0.98;duration:root.still?0:120;easing.type:Easing.InCubic}
 }}
}
