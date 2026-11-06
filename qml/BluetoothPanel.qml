import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var radio
    signal advancedRequested()
    spacing: 14
    Component.onCompleted: radio.startBluetooth()
    Component.onDestruction: radio.stopBluetooth()
    RowLayout {
        Text { text: "Bluetooth"; color:SettingsService.colors.text; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true }
        BusyIndicator { running: root.radio.bluetoothBusy; visible: running; implicitWidth: 28; implicitHeight: 28 }
        Switch {
            text: "On"; checked: root.radio.bluetoothEnabled; enabled: root.radio.bluetoothAvailable
            onToggled: root.radio.setBluetoothEnabled(checked)
            palette.windowText:SettingsService.colors.text; Accessible.name: "Bluetooth power"
        }
    }
    Text { Layout.fillWidth: true; text: root.radio.bluetoothStatus; textFormat: Text.PlainText; color:SettingsService.colors.text; font.pixelSize: 13; wrapMode: Text.WordWrap }
    Repeater {
        model: root.radio.bluetoothDevices
        Rectangle {
            required property var modelData
            id:deviceCard
            property bool details:false
            Layout.fillWidth: true; implicitHeight: details?126:88; radius: 13
            color: modelData.connected ? "#405E387F" :SettingsService.colors.text; border.color:SettingsService.colors.text
            RowLayout {
                anchors.fill: parent; anchors.margins: 13; spacing: 12
                ConnectionGlyph { bluetooth: true; connected: modelData.connected; Layout.preferredWidth: 30; Layout.preferredHeight: 28 }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { Layout.fillWidth: true; text: modelData.name; textFormat: Text.PlainText; color:SettingsService.colors.text; font.pixelSize: 15; elide: Text.ElideRight }
                    Text { text: (modelData.connected?"Connected":modelData.paired?"Paired":"Ready to pair")+(modelData.category?" · "+modelData.category:"")+(modelData.battery>=0?" · "+modelData.battery+"% battery":""); color:SettingsService.colors.muted; font.pixelSize: 11 }
                    Text {visible:deviceCard.details;text:"Address: "+modelData.address;textFormat:Text.PlainText;color:SettingsService.colors.muted;font.pixelSize:11}
                    Text {visible:deviceCard.details&&!!modelData.advertisedName;text:"Advertised name: "+(modelData.advertisedName||"");textFormat:Text.PlainText;color:SettingsService.colors.muted;font.pixelSize:11}
                }
                SovelButton {
                    text:"⋯";Accessible.name:"Device options for "+modelData.name
                    onClicked:{deviceOptions.device=deviceCard.modelData;deviceOptions.card=deviceCard;deviceOptions.popup()}
                }
                SovelButton {
                    text: modelData.connected?"Disconnect":modelData.paired?"Connect":"Pair"
                    enabled: !root.radio.bluetoothBusy
                    onClicked: {if(modelData.connected)root.radio.disconnectBluetooth(modelData.path);else root.radio.connectBluetooth(modelData.path)}
                    Accessible.name: text+" "+modelData.name
                }
                SovelButton {
                    text: "Forget"; visible: modelData.paired; enabled: !root.radio.bluetoothBusy
                    onClicked: {forgetDialog.device=modelData;forgetDialog.open()}
                }
            }
        }
    }
    RowLayout {
        SovelButton { text: "Scan again"; enabled: !root.radio.bluetoothBusy; onClicked: root.radio.scanBluetooth() }
        SovelButton { text: "Advanced Bluetooth"; onClicked: root.advancedRequested() }
    }
    ShellMenu {
        id:deviceOptions;property var device:({});property var card:null
        ShellMenuItem {text:"Rename…";enabled:!!deviceOptions.device.paired;onTriggered:{renameDialog.device=deviceOptions.device;newName.text=deviceOptions.device.name;renameDialog.open()}}
        ShellMenuItem {text:deviceOptions.card&&deviceOptions.card.details?"Hide details":"Advanced details";onTriggered:if(deviceOptions.card)deviceOptions.card.details=!deviceOptions.card.details}
    }
    Dialog {
        id:renameDialog;property var device:({});title:"Rename Bluetooth device";modal:true;anchors.centerIn:Overlay.overlay
        width:Math.min(410,Overlay.overlay?Overlay.overlay.width-32:410);padding:24;standardButtons:Dialog.Ok|Dialog.Cancel
        background:ShellGlass {radius:18;tint:0.9}
        contentItem:ColumnLayout {spacing:12
            Text {Layout.fillWidth:true;text:"Choose a name you will recognize. Leave empty to restore the advertised name.";color:SettingsService.colors.text;wrapMode:Text.Wrap}
            TextField {id:newName;Layout.fillWidth:true;maximumLength:80;selectByMouse:true;Accessible.name:"Bluetooth device name";onAccepted:renameDialog.accept()}
        }
        onAccepted:root.radio.renameBluetooth(device.path,newName.text)
    }
    Dialog {
        id: forgetDialog; property var device: ({name:"",path:""})
        title: "Forget device?"; modal: true; anchors.centerIn: Overlay.overlay
        width: Math.min(410,Overlay.overlay?Overlay.overlay.width-32:410); height: 220
        padding: 24; standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { radius: 18; color:SettingsService.colors.surface; border.color:SettingsService.colors.muted }
        contentItem: Text { text: "You will need to pair "+forgetDialog.device.name+" again to reconnect."; textFormat: Text.PlainText; color:SettingsService.colors.text; wrapMode: Text.WordWrap }
        onAccepted: root.radio.forgetBluetooth(device.path)
    }
}
