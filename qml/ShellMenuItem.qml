import QtQuick
import QtQuick.Controls
MenuItem {
 id:control
 readonly property bool themed:typeof SettingsService!=="undefined"&&!!SettingsService;implicitHeight:visible?34:0;implicitWidth:Math.max(210,label.implicitWidth+50)
 leftPadding:checkable?30:12;rightPadding:18
 contentItem:Text {id:label;text:control.text;font:control.font;color:control.themed?(control.enabled?SettingsService.colors.text:SettingsService.colors.muted):control.enabled?"#F8F0FF":"#A897BD";verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
 background:Rectangle {radius:9;color:control.highlighted?"#628B50B4":"transparent";border.color:control.activeFocus?"#B8DCC4FF":"transparent";Behavior on color {ColorAnimation {duration:System.reducedMotion?0:90}}}
 indicator:Text {x:10;y:(control.height-height)/2;text:control.checked?"✓":"";color:"#F7E9FF"}
}
