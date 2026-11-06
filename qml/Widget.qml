import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root
    property string kind: "clock"
    property bool editing: false
    property string clock: Qt.formatTime(new Date(),"h:mm AP")
    signal moved()
    width: 290
    height: kind === "jared" ? 160 : kind === "system" ? 174 : kind === "actions" ? 104 : 132
    scale: mover.active && !System.reducedMotion ? 1.035 : 1
    Behavior on scale { enabled: !System.reducedMotion; SpringAnimation { spring: 4; damping: 0.3; epsilon: 0.001 } }
    ShellGlass { anchors.fill: parent }
    DragHandler {
        id: mover; enabled: root.editing; target: root
        xAxis.minimum: 12; xAxis.maximum: Math.max(12,root.parent.width-root.width-12)
        yAxis.minimum: 60; yAxis.maximum: Math.max(60,root.parent.height-root.height-120)
        onActiveChanged: if(!active){root.x=Math.round(root.x/12)*12;root.y=Math.round(root.y/12)*12;root.moved();}
    }
    Rectangle { anchors.fill: parent; radius: 24; visible: root.editing; color: "transparent"; border.color: "#BF40FA"; border.width: 2 }
    Timer { interval: 1000; repeat: true; running: root.kind === "clock"; onTriggered: root.clock=Qt.formatTime(new Date(),"h:mm AP") }
    Column {
        anchors.fill: parent; anchors.margins: 20; spacing: 10
        visible: root.kind === "clock"
        Text { text: Qt.formatDate(new Date(),"dddd, MMMM d"); color: "#CBC1E4"; font.pixelSize: 13 }
        Text { text: root.clock; color: "#F2EDFF"; font.pixelSize: 34; font.weight: Font.Medium }
        Text { text: "Welcome, " + System.username + "."; color: "#CBC1E4"; font.pixelSize: 13 }
    }
    Column {
        anchors.fill: parent; anchors.margins: 18; spacing: 8; visible: root.kind === "jared"
        Text { text: "✦   JARED"; color: "#E3D9FC"; font.pixelSize: 17; font.weight: Font.DemiBold }
        Text { text: "Local assistant"; color: "#B8B0CF"; font.pixelSize: 13 }
        Text { text: "Ask questions and approve\nchanges on this computer."; color: "#B8B0CF"; font.pixelSize: 12 }
        SovelButton { text: "Open JARED"; onClicked: System.launch("jared") }
    }
    Column {
        anchors.fill: parent; anchors.margins: 20; spacing: 12; visible: root.kind === "system"
        Text { text: "System Overview"; color: "#E3D9FC"; font.pixelSize: 14 }
        Grid {
            columns: 2; spacing: 12
            Repeater {
                model: [
                    {label:"CPU",value: System.metrics.cpu >= 0 ? System.metrics.cpu.toFixed(0)+"%" : "Sampling…"},
                    {label:"GPU",value:"Unavailable"},
                    {label:"Memory",value: Number(System.metrics.ramUsed||0).toFixed(1)+" / "+Number(System.metrics.ramTotal||0).toFixed(0)+" GB"},
                    {label:"Free storage",value:Number(System.metrics.diskFree||0).toFixed(0)+" GB"}]
                Rectangle {
                    required property var modelData
                    width: 118; height: 52; radius: 12; color: "#18FFFFFF"; border.color: "#25C4ABFF"
                    Column { anchors.centerIn: parent; spacing: 4
                        Text { text: modelData.label; color: "#B8B0CF"; font.pixelSize: 10 }
                        Text { text: modelData.value; color: "#F2EDFF"; font.pixelSize: 13 }
                    }
                }
            }
        }
    }
    Column {
        anchors.fill: parent; anchors.margins: 18; spacing: 12; visible: root.kind === "actions"
        Text { text: "Quick Actions"; color: "#E3D9FC"; font.pixelSize: 12 }
        Row { spacing: 8
            SovelButton { text: "Files"; onClicked: System.launch("files") }
            SovelButton { text: "Settings"; onClicked: System.launch("settings") }
            SovelButton { text: "Lock"; onClicked: System.launch("lock") }
        }
    }
}
