import QtQuick
import QtQuick.Controls
Popup {
 id:root;popupType:Popup.Item;focus:true;padding:16;margins:8
 closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside|Popup.CloseOnReleaseOutside
 background:ShellGlass {width:root.width;height:root.height;radius:18;tint:0.7}
 onAboutToShow:{System.claimMenu(root);if(typeof Chrome!=="undefined")Chrome.registerPopup(root,true)}
 Component.onDestruction:if(typeof Chrome!=="undefined")Chrome.registerPopup(root,false)
 onClosed:if(typeof Chrome!=="undefined")Chrome.registerPopup(root,false)
 Connections {target:sovelWindow;function onActiveChanged(){if(!sovelWindow.active&&root.visible)root.close()}}
 Connections {target:Qt.application;function onStateChanged(){if(Qt.application.state!==Qt.ApplicationActive)root.close()}}
 enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:System.reducedMotion?0:120;easing.type:Easing.OutCubic}}
 exit:Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:System.reducedMotion?0:90}}
}
