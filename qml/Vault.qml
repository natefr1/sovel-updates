import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// Private Mode only: the binary refuses `--app vault` in Normal Mode.
Item {
    id: root
    width: 560; height: 460
    readonly property var vault: System.vault || ({})
    readonly property bool ready: !!vault.initialized
    readonly property bool open: !!vault.mounted
    property string masterKey: ""
    Component.onCompleted: System.refreshVault()
    Connections { target: System; function onVaultChanged(){ if(root.vault.masterKey)root.masterKey=root.vault.masterKey; } }
    Rectangle { anchors.fill: parent; color: SettingsService.colors.background }
    // Instrument frame corners.
    Repeater {
        model: 4
        Item {
            required property int index
            width: 22; height: 22; x: index % 2 ? root.width - 34 : 12; y: index < 2 ? 12 : root.height - 34
            Rectangle { width: parent.width; height: 1; color: "#80BF40FA"; y: index < 2 ? 0 : parent.height - 1 }
            Rectangle { width: 1; height: parent.height; color: "#80BF40FA"; x: index % 2 ? parent.width - 1 : 0 }
        }
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 36; spacing: 16
        RowLayout {
            spacing: 14
            Image { source: "qrc:/assets/icons/vault.svg"; Layout.preferredWidth: 54; Layout.preferredHeight: 54; sourceSize: Qt.size(108,108) }
            ColumnLayout { spacing: 3
                Text { text: "Vault"; color: SettingsService.colors.text; font.pixelSize: 26; font.weight: Font.DemiBold }
                Text { text: (root.open ? "OPEN" : root.ready ? "SEALED" : "NOT CREATED") + "  ·  AES-256-GCM"; color: root.open ? "#5ACFE7" : SettingsService.colors.accent; font.family: "monospace"; font.pixelSize: 11; font.letterSpacing: 1.5 }
            }
        }
        Text {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: SettingsService.colors.muted; font.pixelSize: 13
            text: root.open ? "Your Vault is open at " + (root.vault.mountpoint || "~/Vault") + ". It closes after 15 idle minutes and whenever Sovel locks."
                : root.ready ? "Enter the Vault passphrase. It is separate from your private account password."
                : "Create a second encrypted folder inside the private workspace. Choose a passphrase of at least 12 characters that you do not use anywhere else."
        }
        TextField {
            id: passphrase; Layout.fillWidth: true; Layout.preferredHeight: 42; visible: !root.open && !root.masterKey
            echoMode: TextInput.Password; placeholderText: root.ready ? "Vault passphrase" : "New Vault passphrase"; placeholderTextColor: SettingsService.colors.muted; enabled: !System.vaultBusy
            color: SettingsService.colors.text; font.family: "monospace"; focus: true
            background: Rectangle { radius: 12; color: SettingsService.colors.field; border.color: passphrase.activeFocus ? SettingsService.colors.accent : SettingsService.colors.border }
            onAccepted: primary.clicked()
            Accessible.name: "Vault passphrase"
        }
        TextField {
            id: confirmation; Layout.fillWidth: true; Layout.preferredHeight: 42; visible: !root.ready && !root.masterKey
            echoMode: TextInput.Password; placeholderText: "Repeat the passphrase"; placeholderTextColor: SettingsService.colors.muted; enabled: !System.vaultBusy
            color: SettingsService.colors.text; font.family: "monospace"
            background: Rectangle { radius: 12; color: SettingsService.colors.field; border.color: confirmation.activeFocus ? SettingsService.colors.accent : SettingsService.colors.border }
            onAccepted: primary.clicked()
            Accessible.name: "Repeat Vault passphrase"
        }
        Rectangle {
            visible: !!root.masterKey; Layout.fillWidth: true; implicitHeight: keyColumn.implicitHeight + 24; radius: 12
            color: "#1A5ACFE7"; border.color: "#805ACFE7"
            ColumnLayout { id: keyColumn; anchors.fill: parent; anchors.margins: 12; spacing: 6
                Text { text: "Vault master key — write it down and keep it offline"; color: SettingsService.colors.text; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
                TextEdit { text: root.masterKey; readOnly: true; selectByMouse: true; color: "#5ACFE7"; font.family: "monospace"; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: TextEdit.WrapAnywhere }
                Text { text: "It recovers the Vault if the passphrase is lost. Sovel will not show it again."; color: SettingsService.colors.muted; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.Wrap }
            }
        }
        Text { Layout.fillWidth: true; visible: !!root.vault.error || !!root.vault.message; wrapMode: Text.Wrap; font.pixelSize: 12
            text: root.vault.error || root.vault.message || ""; color: root.vault.error ? SettingsService.colors.danger : SettingsService.colors.muted }
        Item { Layout.fillHeight: true }
        RowLayout {
            spacing: 10
            SovelButton {
                id: primary; primary: true; enabled: !System.vaultBusy
                text: root.masterKey ? "I've stored the key" : root.open ? "Lock Vault" : root.ready ? "Unlock" : "Create Vault"
                onClicked: {
                    if(root.masterKey){ root.masterKey=""; System.refreshVault(); return }
                    if(root.open){ System.vaultAction("lock"); return }
                    if(!root.ready && passphrase.text !== confirmation.text){ confirmation.clear(); confirmation.placeholderText="The passphrases did not match"; return }
                    const secret=passphrase.text; passphrase.clear(); confirmation.clear()
                    System.vaultAction(root.ready ? "unlock" : "init", secret)
                }
            }
            SovelButton { visible: root.open; text: "Show in Files"; onClicked: System.openFile(root.vault.mountpoint) }
            Item { Layout.fillWidth: true }
            BusyIndicator { running: System.vaultBusy; visible: running; Layout.preferredWidth: 28; Layout.preferredHeight: 28 }
        }
    }
}
