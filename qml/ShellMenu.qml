import QtQuick
import QtQuick.Controls
Menu {
 id:root;popupType:Popup.Item;padding:7;focus:true
 property int lastActiveId:0
 closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside|Popup.CloseOnReleaseOutside
 implicitWidth:Math.max(220,contentItem.implicitWidth+leftPadding+rightPadding)
 background:ShellGlass {radius:16;tint:0.58}
 delegate:ShellMenuItem {}
 onAboutToShow:{const entry=Windows.entries.find(w=>w.active);lastActiveId=entry?entry.id:0;System.claimMenu(root);if(typeof Chrome!=="undefined")Chrome.registerPopup(root,true)}
 Component.onDestruction:if(typeof Chrome!=="undefined")Chrome.registerPopup(root,false)
 onClosed:if(typeof Chrome!=="undefined")Chrome.registerPopup(root,false)
 Connections {target:typeof sovelWindow!=="undefined"?sovelWindow:null;function onActiveChanged(){if(!sovelWindow.active&&root.visible)root.close()}}
 Connections {target:Windows;function onChanged(){const entry=Windows.entries.find(w=>w.active);const id=entry?entry.id:0;if(root.visible&&id>0&&id!==root.lastActiveId)root.close();root.lastActiveId=id}}
 Connections {target:Qt.application;function onStateChanged(){if(Qt.application.state!==Qt.ApplicationActive)root.close()}}
 enter:Transition {NumberAnimation {property:"opacity";from:0;to:1;duration:System.reducedMotion?0:120;easing.type:Easing.OutCubic}}
 exit:Transition {NumberAnimation {property:"opacity";from:1;to:0;duration:System.reducedMotion?0:90;easing.type:Easing.InCubic}}
}
