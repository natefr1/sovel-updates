import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    required property var radio
    signal advancedRequested()
    spacing: 14
    Component.onCompleted: radio.startWifi()
    Component.onDestruction: radio.stopWifi()
    RowLayout {
        Layout.fillWidth: true
        Text { text: "Wi-Fi"; color:SettingsService.colors.text; font.pixelSize: 22; font.weight: Font.DemiBold; Layout.fillWidth: true }
        BusyIndicator { running: root.radio.wifiBusy; visible: running; implicitWidth: 28; implicitHeight: 28 }
        Switch {
            text: "On"; checked: root.radio.wifiEnabled; enabled: root.radio.wifiAvailable
            onToggled: root.radio.setWifiEnabled(checked)
            palette.windowText:SettingsService.colors.text; Accessible.name: "Wi-Fi power"
        }
    }
    Text {
        Layout.fillWidth: true; text: root.radio.wifiStatus; textFormat: Text.PlainText
        color:SettingsService.colors.text; font.pixelSize: 13; wrapMode: Text.WordWrap
    }
    Repeater {
        model: root.radio.wifiNetworks
        Rectangle {
            required property var modelData
            Layout.fillWidth: true; implicitHeight: 72; radius: 13
            color: modelData.connected ? "#405E387F" :SettingsService.colors.text; border.color:SettingsService.colors.text
            RowLayout {
                anchors.fill: parent; anchors.margins: 13; spacing: 12
                ConnectionGlyph { connected: modelData.connected; strength: modelData.strength; Layout.preferredWidth: 30; Layout.preferredHeight: 28 }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { Layout.fillWidth: true; text: modelData.ssid; textFormat: Text.PlainText; color:SettingsService.colors.text; font.pixelSize: 15; elide: Text.ElideRight }
                    Text { text: (modelData.connected ? "Connected · " : "")+modelData.security+" · "+modelData.strength+"%"+(modelData.frequency?" · "+(modelData.frequency<3000?"2.4":modelData.frequency<5925?"5":"6")+" GHz":""); color:SettingsService.colors.muted; font.pixelSize: 11 }
                }
                SovelButton {
                    text: modelData.connected ? "Disconnect" : "Connect"
                    enabled: !root.radio.wifiBusy
                    onClicked: {
                        if(modelData.connected)root.radio.disconnectWifi(modelData.path)
                        else if(modelData.hidden)hiddenDialog.open()
                        else if(modelData.advanced)root.advancedRequested()
                        else if(root.radio.savedWifi.some(p=>p.ssid===modelData.ssid))root.radio.connectSavedWifi(root.radio.savedWifi.find(p=>p.ssid===modelData.ssid).path)
                        else if(modelData.needsPassword){passwordDialog.network=modelData;passwordDialog.open()}
                        else root.radio.connectWifi(modelData.path, "")
                    }
                    Accessible.name: "Connect to "+modelData.ssid
                }
            }
        }
    }
    RowLayout {
        SovelButton { text: "Scan again"; enabled: !root.radio.wifiBusy; onClicked: root.radio.scanWifi() }
        SovelButton { text: "Hidden network…"; enabled: root.radio.wifiEnabled&&!root.radio.wifiBusy; onClicked: hiddenDialog.open() }
        SovelButton { text: "Advanced connections"; onClicked: root.advancedRequested() }
    }
    Text {visible:root.radio.savedWifi.length>0;text:"Saved networks";color:SettingsService.colors.text;font.pixelSize:17;font.bold:true}
    Repeater {model:root.radio.savedWifi
        RowLayout {required property var modelData;Layout.fillWidth:true
            ColumnLayout {Layout.fillWidth:true
                Text {Layout.fillWidth:true;text:modelData.name;textFormat:Text.PlainText;color:SettingsService.colors.text;elide:Text.ElideRight}
                Text {text:(modelData.hidden?"Hidden network · ":"")+(modelData.autoconnect?"Automatic reconnect":"Manual reconnect");color:SettingsService.colors.muted;font.pixelSize:11}
            }
            SovelButton {text:"Connect";enabled:!root.radio.wifiBusy&&root.radio.wifiEnabled;onClicked:root.radio.connectSavedWifi(modelData.path)}
            SovelButton {text:"Forget…";enabled:!root.radio.wifiBusy;onClicked:{forgetDialog.network=modelData;forgetDialog.open()}}
        }
    }
    Dialog {id:forgetDialog;property var network:({});title:"Forget this network?";modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(400,root.width);height:240;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {tint:.9}
        contentItem:Text {text:"Remove the saved credentials for "+(forgetDialog.network.name||"this network")+"? An active connection may continue until disconnected.";wrapMode:Text.Wrap;color:SettingsService.colors.text;textFormat:Text.PlainText}
        onAccepted:root.radio.forgetWifi(network.path)
    }
    Dialog {id:hiddenDialog;title:"Add hidden network";modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(440,Overlay.overlay?Overlay.overlay.width-32:440);height:330;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {tint:.9}
        contentItem:ColumnLayout {spacing:12
            TextField {id:hiddenName;Layout.fillWidth:true;placeholderText:"Exact network name";maximumLength:32;color:SettingsService.colors.text;Accessible.name:"Hidden network name"}
            SovelCombo {id:hiddenSecurity;Layout.fillWidth:true;model:["WPA / WPA2 Personal","WPA3 Personal","Open"]}
            TextField {id:hiddenPassword;Layout.fillWidth:true;visible:hiddenSecurity.currentIndex!==2;placeholderText:"Network password";echoMode:TextInput.Password;color:SettingsService.colors.text;inputMethodHints:Qt.ImhSensitiveData|Qt.ImhNoPredictiveText;Accessible.name:"Hidden network password"}
            Text {Layout.fillWidth:true;text:"Enterprise networks can be configured in Advanced connections. Passwords are sent directly to the network service.";wrapMode:Text.Wrap;color:SettingsService.colors.text;font.pixelSize:12}
        }
        onOpened:{hiddenName.clear();hiddenPassword.clear();hiddenSecurity.currentIndex=0;hiddenName.forceActiveFocus()}
        onAccepted:{root.radio.connectHiddenWifi(hiddenName.text,hiddenPassword.text,["wpa-psk","sae","open"][hiddenSecurity.currentIndex]);hiddenPassword.clear()}
        onRejected:hiddenPassword.clear()
    }
    Dialog {
        id: passwordDialog
        property var network: ({ssid:"",path:""})
        title: "Connect to Wi-Fi"; modal: true
        anchors.centerIn: Overlay.overlay
        width: Math.min(420,Overlay.overlay ? Overlay.overlay.width-32 : 420)
        height: 330
        padding: 24; standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { radius: 18; color:SettingsService.colors.surface; border.color:SettingsService.colors.muted }
        contentItem: ColumnLayout {
            spacing: 14
            Text { Layout.fillWidth: true; text: passwordDialog.network.ssid; textFormat: Text.PlainText; color: "white"; font.pixelSize: 18; wrapMode: Text.WordWrap }
            TextField {
                id: wifiPassword; Layout.fillWidth: true; placeholderText: "Network password"
                echoMode: TextInput.Password; color: "white"
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                Accessible.name: "Wi-Fi password"
                onAccepted: { if(text.length>=8)passwordDialog.accept() }
            }
            CheckBox { text: "Show password"; palette.windowText:SettingsService.colors.text; onToggled: wifiPassword.echoMode=checked?TextInput.Normal:TextInput.Password }
            Text { Layout.fillWidth: true; text: "This network will reconnect automatically."; color:SettingsService.colors.text; font.pixelSize: 12; wrapMode: Text.WordWrap }
        }
        onOpened: { wifiPassword.clear();wifiPassword.forceActiveFocus() }
        onAccepted: { root.radio.connectWifi(network.path,wifiPassword.text);wifiPassword.clear() }
        onRejected: wifiPassword.clear()
    }
}
