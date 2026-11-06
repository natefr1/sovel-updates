import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: panel
    required property bool activityOnly
    signal navigate(string target)
    readonly property var events: Jared.messages.filter(item => item.role === "action").slice().reverse()
    spacing: 10

    SettingsLabel {
        text: panel.activityOnly ? "Activity" : "Command Center"
        font.pixelSize: 24
        font.bold: true
    }
    Rectangle {
        visible: !panel.activityOnly
        Layout.fillWidth: true
        implicitHeight: operation.implicitHeight + 24
        radius: 12
        color: SettingsService.colors.surface
        border.color: SettingsService.colors.border
        ColumnLayout {
            id: operation
            anchors.fill: parent
            anchors.margins: 12
            SettingsLabel { text: Jared.sessionState.state.toUpperCase(); color: SettingsService.colors.accent; font.bold: true }
            SettingsLabel { Layout.fillWidth: true; text: Jared.status; wrapMode: Text.Wrap }
            ProgressBar { visible: Jared.downloadProgress >= 0; Layout.fillWidth: true; from: 0; to: 100; value: Jared.downloadProgress }
            Flow {
                Layout.fillWidth: true
                spacing: 8
                SovelButton { text: "Ask JARED"; primary: true; onClicked: panel.navigate("chat") }
                SovelButton { text: "Projects"; onClicked: panel.navigate("projects") }
                SovelButton { text: "Stop"; visible: Jared.busy; onClicked: Jared.cancel() }
            }
        }
    }
    SettingsLabel { text: "Recent actions"; visible: !panel.activityOnly; font.bold: true }
    ListView {
        id: activity
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        model: panel.events
        spacing: 7
        delegate: ColumnLayout {
            required property var modelData
            width: activity.width
            spacing: 3
            SettingsLabel { text: modelData.time ? Qt.formatDateTime(new Date(modelData.time), "ddd h:mm AP") : "Earlier action"; color: SettingsService.colors.muted; font.pixelSize: 11 }
            SettingsLabel { Layout.fillWidth: true; text: modelData.text; textFormat: Text.PlainText; wrapMode: Text.Wrap }
        }
        SettingsLabel { anchors.centerIn: parent; visible: !activity.count; text: "Completed and cancelled actions appear here." }
        ScrollBar.vertical: ScrollBar {}
    }
}
