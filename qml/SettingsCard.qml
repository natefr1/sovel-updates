import QtQuick
import QtQuick.Layouts
Rectangle {
 id:root
 default property alias contents:body.data
 Layout.fillWidth:true
 implicitHeight:body.implicitHeight+32
 radius:17;color:SettingsService.colors.surface;border.color:SettingsService.colors.border
 ColumnLayout {id:body;anchors.fill:parent;anchors.margins:16;spacing:14}
}
