import QtQuick
import QtQuick.Controls
import QtQuick.Window
Item {
    id: frame
    readonly property bool themed:typeof SettingsService!=="undefined"&&!!SettingsService
    width: 1040; height: 720
    property int pendingAction:-1
    opacity:0
    Component.onCompleted:appear.start()
    NumberAnimation {id:appear;target:frame;property:"opacity";to:1;duration:System.reducedMotion?0:150;easing.type:Easing.OutCubic}
    NumberAnimation {id:depart;target:frame;property:"opacity";to:0.15;duration:System.reducedMotion?0:100;easing.type:Easing.InCubic;onFinished:{if(frame.pendingAction===0)sovelWindow.close();else sovelWindow.showMinimized();frame.opacity=1;frame.pendingAction=-1}}
    Rectangle { anchors.fill: parent; radius: 18; color:frame.themed?SettingsService.colors.sidebar:"#241A32"; border.color: "#7066507C" }
    Item {
        id: titlebar; height: 42; width: parent.width
        MouseArea {
            anchors.fill: parent
            onPressed: sovelWindow.startSystemMove()
            onDoubleClicked: frame.toggleSize()
        }
        Row {
            x: 18; anchors.verticalCenter: parent.verticalCenter; spacing: 9
            Repeater {
                model: [{name:"Close",color:"#FF625B",symbol:"×"},{name:"Minimize",color:"#FFBF43",symbol:"−"},{name:"Maximize or restore",color:"#30C94B",symbol:"+"}]
                Button {
                    id:windowControl
                    required property var modelData
                    required property int index
                    width: 16; height: 16; padding: 0
                    background: Rectangle { radius: 8; color: parent.modelData.color; border.color: Qt.darker(color,1.2) }
                    contentItem: Text { text: parent.hovered || parent.activeFocus ? parent.modelData.symbol : ""; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: "#5C2428"; font.pixelSize: 13; font.bold: true }
                    onClicked: {if(index<2){frame.pendingAction=index;depart.start();}else frame.toggleSize();}
                    ShellToolTip {visible:windowControl.hovered;delay:500;text:windowControl.modelData.name}
                    Accessible.name: modelData.name
                }
            }
        }
        Text { anchors.centerIn: parent; text: appTitle; font.pixelSize: 13; font.weight: Font.DemiBold; color:frame.themed?SettingsService.colors.text:"#EAE0F6" }
    }
    Loader { anchors.fill: parent; anchors.topMargin: 42; anchors.margins: 1; source: appPage }
    function toggleSize(){if(sovelWindow.visibility===Window.Maximized)sovelWindow.showNormal();else sovelWindow.showMaximized();}
    Repeater {
        model: [Qt.LeftEdge,Qt.RightEdge,Qt.TopEdge,Qt.BottomEdge]
        MouseArea {
            required property int modelData
            width: modelData===Qt.LeftEdge || modelData===Qt.RightEdge ? 5 : frame.width-10
            height: modelData===Qt.TopEdge || modelData===Qt.BottomEdge ? 5 : frame.height-10
            x: modelData===Qt.RightEdge ? frame.width-5 : modelData===Qt.LeftEdge ? 0 : 5
            y: modelData===Qt.BottomEdge ? frame.height-5 : modelData===Qt.TopEdge ? 0 : 5
            cursorShape: modelData===Qt.LeftEdge || modelData===Qt.RightEdge ? Qt.SizeHorCursor : Qt.SizeVerCursor
            onPressed: sovelWindow.startSystemResize(modelData)
        }
    }
    MouseArea { width: 12; height: 12; anchors.right: parent.right; anchors.bottom: parent.bottom; cursorShape: Qt.SizeFDiagCursor; onPressed: sovelWindow.startSystemResize(Qt.RightEdge|Qt.BottomEdge) }
}
