import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root
    width: 1000; height: 720
    property var apps: System.applications()
    Connections { target: sovelWindow; function onActiveChanged(){if(sovelWindow.active)root.apps=System.applications(true);} }
    Rectangle { anchors.fill: parent; color: "#171221" }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 32; spacing: 20
        RowLayout {
            Layout.fillWidth: true
            Text { text: "App Library"; color: "#F5F0FF"; font.pixelSize: 28; font.weight: Font.DemiBold; Layout.fillWidth: true }
            SovelButton { text: "Open Store"; onClicked: System.launch("store") }
        }
        TextField {
            id: search; Layout.fillWidth: true; placeholderText: "Search installed apps"; focus: true
            color: "#F5F0FF"; placeholderTextColor: "#AEA3C4"; padding: 14
            background: Rectangle { color: "#282034"; radius: 14; border.color: "#625274" }
            Accessible.name: "Search installed apps"
        }
        GridView {
            id: grid; Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            cellWidth: 140; cellHeight: 136
            model: root.apps.filter(app=>app.name.toLowerCase().includes(search.text.toLowerCase()))
            delegate: Button {
                required property var modelData
                width: grid.cellWidth-12; height: grid.cellHeight-12; padding: 8
                text: modelData.name
                background: Rectangle { radius: 18; color: parent.hovered ? "#332A43" : "transparent"; border.color: parent.activeFocus ? "#C7A5FF" : "transparent" }
                contentItem: Column {
                    spacing: 10
                    Image { width: 64; height: 64; anchors.horizontalCenter: parent.horizontalCenter; source: modelData.icon; sourceSize: Qt.size(128,128) }
                    Text { width: parent.width; text: modelData.name; horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight; color: "#F3EBFF"; font.pixelSize: 12 }
                }
                onClicked: {if(!Windows.activateApp(modelData.appIds||[],false))System.launchApplication(modelData.id)}
                TapHandler {acceptedButtons:Qt.RightButton;onTapped:{appMenu.app=modelData;appMenu.popup()}}
                Accessible.name: text
            }
            ScrollBar.vertical: ScrollBar {}
        }
        Text { visible: System.notice.length>0; text: System.notice; color: "#DAC4F4"; wrapMode: Text.Wrap; Layout.fillWidth: true }
    }
    Shortcut { sequence: "Ctrl+F"; onActivated: search.forceActiveFocus() }
    ShellMenu {
        id:appMenu;property var app:({id:"",name:"",appIds:[]})
        ShellMenuItem {text:"Open "+appMenu.app.name;onTriggered:{if(!Windows.activateApp(appMenu.app.appIds||[],false))System.launchApplication(appMenu.app.id)}}
        ShellMenuItem {text:{let pins=System.pinnedApps;return System.isPinned(appMenu.app.id)?"Unpin from Dock":"Pin to Dock"} onTriggered:System.setPinned(appMenu.app.id,!System.isPinned(appMenu.app.id))}
    }
    Shortcut { sequence: "F5"; onActivated: root.apps=System.applications(true) }
}
