import QtQuick
import QtQuick.Controls
Button {
    id: control
    readonly property bool themed:typeof SettingsService!=="undefined"&&!!SettingsService
    property bool primary: false
    property string glyph: ""
    implicitHeight: 40
    implicitWidth: Math.max(40, label.implicitWidth + 30)
    hoverEnabled: true
    opacity: enabled ? 1 : 0.4
    scale: down && !System.reducedMotion ? 0.96 : 1
    Behavior on scale { enabled: !System.reducedMotion; SpringAnimation { spring: 4; damping: 0.32; epsilon: 0.001 } }
    contentItem: Text {
        id: label
        text: control.glyph ? control.glyph + "  " + control.text : control.text
        color: control.themed?(control.primary?SettingsService.colors.accentText:SettingsService.colors.text):"#F2EDFF"; font.pixelSize: 13
        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 12
        color: control.themed?(control.primary?SettingsService.colors.accent:control.hovered?SettingsService.colors.surfaceHover:SettingsService.colors.surface):control.primary ? (control.hovered ? "#AE42EE" : "#7829D4") : control.hovered ? "#35CAB6FF" : "#15FFFFFF"
        border.color: control.activeFocus ? "#E3D9FC" : "#30C8ABFF"
        border.width: control.activeFocus ? 2 : 1
        Behavior on color { ColorAnimation { duration: System.reducedMotion ? 0 : 160 } }
    }
    Accessible.name: text
}
