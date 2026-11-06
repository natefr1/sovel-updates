import QtQuick
import QtQuick.Controls
Button {
 id:root;checkable:true;implicitWidth:48;implicitHeight:28;padding:0
 Accessible.role:Accessible.CheckBox
 background:Rectangle {radius:14;color:root.checked?SettingsService.colors.accent:SettingsService.colors.border;opacity:root.enabled?1:0.4
  Rectangle {width:22;height:22;radius:11;x:root.checked?parent.width-width-3:3;y:3;color:root.checked?SettingsService.colors.accentText:SettingsService.colors.surface
   Behavior on x {NumberAnimation {duration:System.reducedMotion?0:150;easing.type:Easing.OutCubic}}
  }
 }
 contentItem:Item {}
}
