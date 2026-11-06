import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root
    spacing: 18
    Component.onCompleted:{if(typeof Updates.refreshReleaseInfo==="function")Updates.refreshReleaseInfo();if(Updates.automaticChecks&&!Updates.busy)Updates.check()}
    readonly property bool current: Updates.checked&&!Updates.busy&&Updates.packages.length===0&&!Updates.error.length&&!Updates.restartRequired
    readonly property string headline: Updates.busy?(Updates.installing?"Installing updates…":"Checking for updates…"):Updates.error.length?"Updates need attention.":Updates.restartRequired?"Restart to finish updating.":root.current?"Your Sovel is up to date.":Updates.packages.length?"Sovel updates are available.":"Keep Sovel up to date."
    readonly property var release:Updates.releaseDetails||({})
    function timestamp(value){if(!value)return "Not recorded";const parsed=new Date(value);return isNaN(parsed.getTime())?"Not recorded":Qt.formatDateTime(parsed,"ddd, MMM d yyyy · h:mm AP")}
    SettingsCard {visible:!!root.release.version
        RowLayout {Layout.fillWidth:true;SovelSymbol {symbol:"updates";Layout.preferredWidth:36;Layout.preferredHeight:36;color:SettingsService.colors.accent}ColumnLayout {Layout.fillWidth:true;SettingsLabel {text:(root.release.name||"Sovel")+" · "+root.release.version;font.pixelSize:22;font.bold:true}SettingsLabel {text:(root.release.type||"System update")+" · Public channel";color:SettingsService.colors.muted}}}
        Repeater {model:root.release.notes||[];SettingsLabel {required property string modelData;text:"• "+modelData;font.pixelSize:13}}
        Repeater {model:[{label:"Built",value:root.release.builtAt},{label:"Published",value:root.release.publishedAt},{label:"Downloaded",value:root.release.downloadedAt},{label:"Installed",value:root.release.installedAt}]
            RowLayout {required property var modelData;Layout.fillWidth:true;SettingsLabel {Layout.fillWidth:false;Layout.preferredWidth:110;text:modelData.label;color:SettingsService.colors.muted}SettingsLabel {text:root.timestamp(modelData.value);font.pixelSize:12}}
        }
        SettingsLabel {text:root.release.publicationSource||"Publication time is shown only when supplied by signed repository metadata. Older or unobserved timestamps remain unrecorded.";color:SettingsService.colors.muted;font.pixelSize:11}
        SovelButton {text:"Update history";onClicked:updateHistory.open()}
    }
    Dialog {id:updateHistory;title:"Sovel update history";modal:true;anchors.centerIn:Overlay.overlay;width:Math.min(660,sovelWindow.width-40);height:Math.min(580,sovelWindow.height-40);standardButtons:Dialog.Close;background:ShellGlass {tint:.95}
        contentItem:ScrollView {id:updateHistoryScroll;clip:true;contentWidth:availableWidth;ColumnLayout {width:updateHistoryScroll.availableWidth;spacing:14
            Repeater {model:Updates.releaseHistory||[];SettingsCard {required property var modelData
                SettingsLabel {text:(modelData.name||"Sovel")+" · "+modelData.version;font.bold:true}
                SettingsLabel {text:modelData.type||"System update";color:SettingsService.colors.muted}
                SettingsLabel {text:"Built: "+root.timestamp(modelData.builtAt)+"\nPublished: "+root.timestamp(modelData.publishedAt)+"\nDownloaded: "+root.timestamp(modelData.downloadedAt)+"\nInstalled: "+root.timestamp(modelData.installedAt);font.pixelSize:12}
                Repeater {model:modelData.notes||[];SettingsLabel {required property string modelData;text:"• "+modelData;font.pixelSize:12}}
            }}
            SettingsLabel {visible:!(Updates.releaseHistory||[]).length;text:"No installation history has been recorded by this release yet.";color:SettingsService.colors.muted}
        }}
    }

    Rectangle {
        Layout.fillWidth: true; implicitHeight: statusGrid.implicitHeight+36
        radius: 17; color:SettingsService.colors.surface; border.color:SettingsService.colors.border
        GridLayout {
            id: statusGrid; anchors.fill: parent; anchors.margins: 18
            columns: root.width>560?2:1; rowSpacing: 16; columnSpacing: 20
            RowLayout {
                Layout.fillWidth: true; spacing: 14
                Rectangle {
                    Layout.preferredWidth: 48; Layout.preferredHeight: 48; radius: 13
                    color: root.current?"#38B76C":Updates.error.length?"#A5627C":"#9665CE"
                    BusyIndicator { anchors.centerIn: parent; width: 36; height: 36; running: Updates.busy; visible: running; palette.dark: "white" }
                    Canvas {
                        id: statusIcon; anchors.fill: parent; anchors.margins: 11; visible: !Updates.busy
                        property bool isCurrent: root.current
                        property bool attention: Updates.error.length>0
                        onIsCurrentChanged: requestPaint()
                        onAttentionChanged: requestPaint()
                        onPaint: {
                            let c=getContext("2d");c.reset();c.strokeStyle="#FFFFFF";c.lineWidth=2.4;c.lineCap="round";c.lineJoin="round";
                            if(isCurrent){c.beginPath();c.arc(13,13,12,0,2*Math.PI);c.fillStyle="#FFFFFF";c.fill();c.strokeStyle="#38B76C";c.beginPath();c.moveTo(7,13);c.lineTo(11,17);c.lineTo(19,8);c.stroke();}
                            else if(attention){c.beginPath();c.moveTo(13,5);c.lineTo(13,15);c.stroke();c.beginPath();c.arc(13,21,1.3,0,2*Math.PI);c.fillStyle="#FFFFFF";c.fill();}
                            else {c.beginPath();c.moveTo(13,3);c.lineTo(13,19);c.moveTo(7,13);c.lineTo(13,19);c.lineTo(19,13);c.moveTo(4,22);c.lineTo(22,22);c.stroke();}
                        }
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text { objectName: "updates-headline"; Layout.fillWidth: true; text: root.headline; color:SettingsService.colors.text; font.pixelSize: 17; font.weight: Font.Medium; wrapMode: Text.WordWrap }
                    Text { text: "Sovel "+(System.systemInfo.version||"0.1.0")+" · Public edition"; color:SettingsService.colors.muted; font.pixelSize: 13 }
                }
            }
            SovelButton { objectName: "updates-check"; Layout.alignment: Qt.AlignRight|Qt.AlignVCenter; text: "Check for Update"; enabled: !Updates.busy; onClicked: Updates.check() }
        }
    }
    Rectangle {
        Layout.fillWidth: true; implicitHeight: 64; radius: 15; color:SettingsService.colors.surface; border.color:SettingsService.colors.border
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 14; spacing: 12
            Text { Layout.fillWidth: true; text: "Automatic Updates"; color:SettingsService.colors.text; font.pixelSize: 15 }
            Text { text: Updates.automaticChecks?"On":"Off"; color:SettingsService.colors.muted; font.pixelSize: 14 }
            ToolButton {
                objectName: "updates-automatic-info"; implicitWidth: 32; implicitHeight: 32
                Accessible.name: "Automatic update options"; onClicked: automaticDialog.open()
                contentItem: Text { text: "i"; color:SettingsService.colors.muted; font.pixelSize: 17; font.italic: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                background: Rectangle { anchors.centerIn: parent; width: 24; height: 24; radius: 12; color: parent.hovered?"#30FFFFFF":"transparent"; border.color:SettingsService.colors.muted }
            }
        }
    }
    Rectangle {
        Layout.fillWidth: true; visible: Updates.busy||Updates.packages.length>0||Updates.error.length>0||Updates.restartRequired
        implicitHeight: taskBody.implicitHeight+36; radius: 15; color:SettingsService.colors.surface; border.color:SettingsService.colors.border
        ColumnLayout {
            id: taskBody; anchors.fill: parent; anchors.margins: 18; spacing: 14
            Text { Layout.fillWidth: true; text: Updates.status; textFormat: Text.PlainText; color:SettingsService.colors.text; font.pixelSize: 13; wrapMode: Text.WordWrap }
            ProgressBar { Layout.fillWidth: true; visible: Updates.busy; indeterminate: Updates.progress<0; value: Math.max(0,Updates.progress)/100; palette.highlight:SettingsService.colors.muted }
            Text { Layout.fillWidth: true; visible: Updates.error.length>0; text: Updates.error; textFormat: Text.PlainText; color:SettingsService.colors.text; font.pixelSize: 13; wrapMode: Text.WordWrap; Accessible.role: Accessible.AlertMessage }
            Flow {
                Layout.fillWidth: true; spacing: 10
                SovelButton { objectName: "updates-install"; text: "Install Updates"; primary: true; visible: Updates.packages.length>0&&!Updates.liveSession; enabled: Updates.canInstall; onClicked: Updates.install() }
                SovelButton { text: "Cancel"; visible: Updates.busy; enabled: Updates.canCancel; onClicked: Updates.cancel() }
                SovelButton { text: "Restart…"; visible: Updates.restartRequired; enabled: !Updates.busy; onClicked: restartDialog.open() }
            }
            CheckBox { id: details; visible: Updates.packages.length>0; text: "Show update details"; palette.windowText:SettingsService.colors.text }
            Repeater {
                model: details.checked?Updates.packages:[]
                ColumnLayout {
                    required property var modelData
                    Layout.fillWidth: true; spacing: 4
                    Text { Layout.fillWidth: true; text: modelData.name+(modelData.security?" · Security update":""); textFormat: Text.PlainText; color:SettingsService.colors.text; font.pixelSize: 13; elide: Text.ElideRight }
                    Text { Layout.fillWidth: true; text: modelData.summary; textFormat: Text.PlainText; color:SettingsService.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
                    Text { text: modelData.version; textFormat: Text.PlainText; color:SettingsService.colors.muted; font.pixelSize: 11 }
                }
            }
        }
    }
    Text { Layout.fillWidth: true; text: Updates.liveSession?"Install Sovel to keep system updates.":"Updates are verified before installation. You choose when to install and restart."; color:SettingsService.colors.muted; font.pixelSize: 12; wrapMode: Text.WordWrap }
    Text { Layout.fillWidth: true; visible: Updates.lastChecked.length>0; text: "Last checked: "+Qt.formatDateTime(new Date(Updates.lastChecked),"MMM d, yyyy · h:mm AP"); color:SettingsService.colors.muted; font.pixelSize: 11; wrapMode: Text.WordWrap }

    Dialog {
        id: automaticDialog; title: "Automatic Updates"; modal: true; anchors.centerIn: Overlay.overlay
        width: Math.min(440,Overlay.overlay?Overlay.overlay.width-32:440); height: 290; standardButtons: Dialog.Close
        background: Rectangle { radius: 18; color:SettingsService.colors.surface; border.color:SettingsService.colors.muted }
        contentItem: ColumnLayout {
            spacing: 16
            Switch { objectName: "updates-automatic-switch"; Layout.fillWidth: true; text: "Check for updates automatically"; checked: Updates.automaticChecks; palette.windowText:SettingsService.colors.text; onToggled: Updates.automaticChecks=checked }
            Text { Layout.fillWidth: true; text: "Sovel checks once a day while you’re signed in. Updates are installed when you choose Install Updates. Your computer never restarts automatically."; color:SettingsService.colors.text; wrapMode: Text.WordWrap; font.pixelSize: 13 }
        }
    }
    Dialog {
        id: restartDialog; title: "Restart Sovel?"; modal: true; anchors.centerIn: Overlay.overlay
        width: Math.min(420,Overlay.overlay?Overlay.overlay.width-32:420); height: 220; standardButtons: Dialog.Ok|Dialog.Cancel
        background: Rectangle { radius: 18; color:SettingsService.colors.surface; border.color:SettingsService.colors.muted }
        contentItem: Text { text: "Save your work first. Your computer will restart to finish applying updates."; color:SettingsService.colors.text; wrapMode: Text.WordWrap }
        onAccepted: Updates.restart()
    }
}
