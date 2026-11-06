import QtQuick
QtObject {
    readonly property color ink: "#040607"
    readonly property color purple: "#4928C2"
    readonly property color accent: "#BF40FA"
    readonly property color text: "#F2EDFF"
    readonly property color muted: "#B8B0CF"
    readonly property color border: "#605F4C80"
    readonly property int radius: 24
    readonly property int duration: System.reducedMotion ? 0 : 320
}
