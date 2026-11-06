import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: panel
    required property string kind
    required property string widgetId
    required property bool privateHidden
    required property var metrics
    readonly property var assistant: Workspace.assistant || ({})
    readonly property var projectList: assistant.projects || []
    readonly property int projectId: (Workspace.data[widgetId] || {}).projectId || 0
    readonly property var project: projectList.find(entry => entry.id === projectId) || ({})
    readonly property var tasks: (assistant.tasks || []).filter(task => task.project === projectId)
    readonly property var recent: (assistant.recent || {})[projectId] || []
    readonly property bool canUse: !assistant.busy && !assistant.approval
    clip: true
    contentWidth: availableWidth

    component Caption: Text {
        Layout.fillWidth: true
        color: SettingsService.colors.text
        font.pixelSize: 12
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }

    ColumnLayout {
        width: panel.availableWidth
        spacing: 8
        Caption { visible: panel.privateHidden && panel.kind !== "ai-monitor"; text: "Hidden by widget privacy"; color: SettingsService.colors.muted }
        ColumnLayout {
            visible: panel.kind === "jared" && !panel.privateHidden
            Layout.fillWidth: true
            Caption { text: (panel.assistant.state || "offline").toUpperCase(); color: SettingsService.colors.accent; font.bold: true }
            Caption { text: panel.assistant.status || "Start JARED to see its action queue." }
            Caption { visible: !!panel.assistant.approval; text: panel.assistant.approval || ""; font.bold: true }
            SovelCombo {
                Layout.fillWidth: true
                visible: (panel.assistant.choices || []).length > 1
                model: panel.assistant.choices || []
                textRole: "description"
                onActivated: Workspace.assistantCommand("choose-output", String(currentIndex))
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                SovelButton { text: "Approve"; visible: !!panel.assistant.approval; primary: true; onClicked: Workspace.assistantCommand("approve", "") }
                SovelButton { text: "Reject"; visible: !!panel.assistant.approval; onClicked: Workspace.assistantCommand("reject", "") }
                SovelButton { text: "Cancel task"; visible: !!panel.assistant.busy; onClicked: Workspace.assistantCommand("cancel", "") }
                SovelButton { text: "Open JARED"; onClicked: Workspace.assistantCommand("show", "") }
            }
        }
        ColumnLayout {
            visible: panel.kind === "projects-deck" && !panel.privateHidden
            Layout.fillWidth: true
            Caption { text: panel.project.name || "Choose a saved JARED project." }
            SovelCombo {
                Layout.fillWidth: true
                visible: panel.projectList.length > 0
                model: panel.projectList
                textRole: "name"
                currentIndex: panel.projectList.findIndex(entry => entry.id === panel.projectId)
                onActivated: {
                    const selected = panel.projectList[currentIndex].id
                    const current = Workspace.data[panel.widgetId] || ({})
                    Workspace.setWidgetData(panel.widgetId, Object.assign({}, current, {projectId: selected}))
                    if (panel.canUse)
                        Workspace.assistantCommand("project-summary", String(selected))
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                SovelButton { text: "Load projects"; enabled: panel.canUse; onClicked: Workspace.assistantCommand("load-workspaces", "") }
                SovelButton { text: "Open folder"; visible: !!panel.project.path; enabled: panel.canUse; onClicked: Workspace.assistantCommand("project-open-file", panel.project.path) }
                SovelButton { text: "Refresh files"; visible: !!panel.project.id; enabled: panel.canUse; onClicked: Workspace.assistantCommand("project-summary", String(panel.projectId)) }
            }
            Caption { visible: panel.tasks.length > 0; text: panel.tasks.filter(task => !!task.done).length + " of " + panel.tasks.length + " tasks done"; color: SettingsService.colors.muted }
            Repeater {
                model: panel.tasks.slice(0, 8)
                CheckBox {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.title
                    checked: !!modelData.done
                    enabled: panel.canUse
                    onClicked: Workspace.assistantCommand("project-task", modelData.id + ":" + (checked ? "1" : "0"))
                }
            }
            Caption { visible: panel.recent.length > 0; text: "Recent files"; color: SettingsService.colors.muted }
            Repeater {
                model: panel.recent
                SovelButton {
                    required property var modelData
                    Layout.fillWidth: true
                    text: modelData.name
                    enabled: panel.canUse
                    onClicked: Workspace.assistantCommand("project-open-file", modelData.path)
                }
            }
        }
        ColumnLayout {
            visible: panel.kind === "ai-monitor"
            Layout.fillWidth: true
            Caption { text: panel.assistant.loaded ? (panel.assistant.runtime || "Local model loaded") : "No active local model"; font.bold: true }
            Caption { text: panel.metrics.cpu >= 0 ? "System CPU  " + Math.round(panel.metrics.cpu) + "%" : "CPU sample unavailable" }
            Caption { text: panel.metrics.gpuAvailable ? (panel.metrics.gpuName || "GPU") + "  " + Math.round(panel.metrics.gpu) + "%" : "GPU telemetry unavailable" }
            ProgressBar { Layout.fillWidth: true; visible: panel.assistant.progress >= 0; from: 0; to: 100; value: panel.assistant.progress || 0 }
            Flow {
                Layout.fillWidth: true
                spacing: 6
                SovelButton { text: "Models"; onClicked: Workspace.assistantCommand("preferences", "") }
                SovelButton { text: "Unload"; enabled: !!panel.assistant.loaded && panel.canUse; onClicked: Workspace.assistantCommand("unload", "") }
            }
        }
    }
}
