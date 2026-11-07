import QtQuick
import QtQuick.Controls
import QtQml.Models
Item {
    id: root
    property string activeApplication: Windows.activeName
    property string clock: Qt.formatDateTime(new Date(),"ddd MMM d   h:mm AP")
    ShellGlass { anchors.fill: parent; radius: 0; tint: Number(root.preferences["bar.opacity"]||75)/100 }
    TapHandler {acceptedButtons:Qt.RightButton;onTapped:barContext.popup()}
    ShellMenu {id:barContext;ShellMenuItem {text:"Settings";onTriggered:System.launch("settings")} ShellMenuItem {text:"App Library";onTriggered:System.launch("library")}}
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: System.privateMode ? "#40BF40FA" : "#1FFFFFFF" }
    // Private Mode: discreet JARED status and active task, centred like an instrument readout.
    Rectangle {
        id: missionStatus
        visible: System.privateMode && root.width >= 980
        readonly property string stateName: root.assistant.state || "offline"
        readonly property bool live: ["listening","speaking","thinking","executing"].includes(stateName)
        anchors.centerIn: parent; height: 22; radius: 11
        width: Math.min(420, missionRow.implicitWidth + 26)
        color: "#66000000"; border.width: 1; border.color: live ? "#8C5ACFE7" : "#3DB98BFF"
        Row { id: missionRow; anchors.centerIn: parent; spacing: 8
            Rectangle { width: 6; height: 6; radius: 3; anchors.verticalCenter: parent.verticalCenter
                color: missionStatus.stateName==="error"?"#FA786D":missionStatus.stateName==="approval"?"#E9B957":missionStatus.live?"#5ACFE7":missionStatus.stateName==="offline"?"#6E6385":"#BF40FA" }
            Text { text: "JARED"; color: "#E3D9FC"; font.pixelSize: 10; font.letterSpacing: 2; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
            Text { text: missionStatus.stateName.toUpperCase(); color: missionStatus.live ? "#5ACFE7" : "#B3A6CC"; font.pixelSize: 10; font.letterSpacing: 1.5; font.family: "monospace"; anchors.verticalCenter: parent.verticalCenter }
            Text { width: Math.min(220, implicitWidth); elide: Text.ElideRight; visible: !!text; text: root.assistant.task || root.assistant.status || ""; color: "#B3A6CC"; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
        }
        TapHandler { onTapped: Workspace.assistantCommand("show","") }
    }
    component BarButton: ToolButton {
        height: 30; padding: 10; topPadding: 0; bottomPadding: 0
        contentItem: Text { text: parent.text; color: SettingsService.colors.text; font.pixelSize: 12; verticalAlignment: Text.AlignVCenter }
        background: Rectangle { radius: 5; color: parent.hovered || parent.down ? "#35FFFFFF" : "transparent" }
    }
    Row {
        anchors.left: parent.left; anchors.leftMargin: 10; spacing: 2
        BarButton {
            implicitWidth: 38; Accessible.name: "Sovel menu"
            contentItem: Image { source: "qrc:/assets/icons/sovel.svg"; sourceSize: Qt.size(23,23); fillMode: Image.PreserveAspectFit }
            onClicked: systemMenu.open()
            ShellMenu { id: systemMenu; y: parent.height
                ShellMenuItem { text: "About Sovel"; onTriggered: System.message(System.systemInfo.system+" · "+System.systemInfo.edition) }
                MenuSeparator {}
                ShellMenuItem { text: "Settings…"; onTriggered: System.launch("settings") }
                ShellMenuItem { text: Updates.packages.length?"Software Update… ("+Updates.packages.length+")":"Software Update…"; onTriggered: System.launch("updates") }
                ShellMenuItem { text: "App Library…"; onTriggered: System.launch("library") }
                ShellMenuItem { text: "Store…"; onTriggered: System.launch("store") }
                ShellMenuItem { text: "Vault…"; visible: System.privateMode; onTriggered: System.launch("vault") }
                ShellMenuItem { text: System.privateConfigured ? "Open Private Workspace…" : "Set Up Private Workspace…"; visible: !System.privateMode; onTriggered: System.launch("enter-private") }
                MenuSeparator {}
                ShellMenuItem { text: System.privateMode ? "Lock Computer" : "Lock Screen"; onTriggered: System.launch("lock") }
                ShellMenuItem { text: "Exit Private Workspace…"; visible: System.privateMode; onTriggered: System.launch("exit-private") }
            }
        }
        Text { text: root.activeApplication; width: Math.min(root.width<700?90:160,implicitWidth+18); height: 30; color: SettingsService.colors.text; font.pixelSize: 12; font.weight: Font.DemiBold; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        BarButton {text:"File";visible:root.width>=900;onClicked:fileMenu.open()
            ShellMenu { id: fileMenu; y: parent.height
                ShellMenuItem { text: "New Files Window"; onTriggered: System.launch("files") }
                MenuSeparator {}
                ShellMenuItem { text: "Close Window"; enabled: Windows.hasWindows; onTriggered: Windows.closeActive() }
            }
        }
        BarButton {text:"View";visible:root.width>=900;onClicked:viewMenu.open()
            ShellMenu { id: viewMenu; y: parent.height
                ShellMenuItem { text: Windows.activeMaximized ? "Restore Window" : "Maximize Window"; enabled: Windows.hasWindows; onTriggered: Windows.toggleMaximized() }
                ShellMenuItem { text: "App Library"; onTriggered: System.launch("library") }
            }
        }
        BarButton {text:"Window";visible:root.width>=900;onClicked:windowMenu.open()
            ShellMenu { id: windowMenu; y: parent.height
                ShellMenuItem { text: "Minimize"; enabled: Windows.hasWindows; onTriggered: Windows.minimizeActive() }
                MenuSeparator {}
                Instantiator {
                    model: Windows.entries
                    delegate: ShellMenuItem { required property var modelData; text: modelData.title || modelData.name; onTriggered: Windows.activate(modelData.id) }
                    onObjectAdded: (index,object)=>windowMenu.insertItem(index+2,object)
                    onObjectRemoved: (index,object)=>windowMenu.removeItem(object)
                }
            }
        }
    }
    readonly property var preferences:SettingsService.preferences
    readonly property var assistant:Workspace.assistant||({})
    // JARED exists only in Private Mode; there the bar also ends with a security lock.
    readonly property var order:String(preferences["bar.order"]||"network,bluetooth,sound,battery,notifications,updates,jared,search,control,calendar").split(",").concat(["downloads"]).concat(System.privateMode?["lock"]:[]).filter((key,index,all)=>["network","bluetooth","sound","battery","notifications","updates","search","control","calendar","downloads"].concat(System.assistantAvailable?["jared"]:[]).concat(System.privateMode?["lock"]:[]).includes(key)&&all.indexOf(key)===index)
    property real popupX:width-400
    function positionPopup(button){popupX=Math.max(8,Math.min(width-380,button.mapToItem(root,0,0).x))}
    function battery(){return (Workspace.telemetry.batteries||[])[0]||({})}
    function iconName(key){return ({network:"network",bluetooth:"bluetooth",sound:"sound",battery:"battery",notifications:"notifications",updates:"updates",jared:"jared",search:"search",control:"control",calendar:"clock",downloads:"downloads",lock:"lock"})[key]}
    function showMenu(key,button){positionPopup(button);if(key==="lock"){System.launch("lock");return}if(key==="network")wifiPopover.open();else if(key==="bluetooth")bluetoothPopover.open();else if(key==="sound")soundPopover.open();else if(key==="calendar")calendarPopover.open();else if(key==="jared"){if(jaredMenu.visible)jaredMenu.close();else jaredMenu.open()}else if(key==="control"||key==="battery"||key==="notifications")controlPopover.open();else System.launch(key==="updates"||key==="downloads"?key:"library")}
    Row {id:statusIcons;anchors.right:parent.right;anchors.rightMargin:12;spacing:2
        Repeater {model:root.order
            BarButton {id:statusButton;required property string modelData
                visible:(modelData==="lock"||(modelData==="downloads"?root.preferences["bar.show.downloads"]===true:root.preferences["bar.show."+modelData]!==false))&&(modelData!=="battery"||root.battery().percent>=0)&&(root.width>=700||["control","calendar","jared","lock"].includes(modelData))
                implicitWidth:modelData==="calendar"?clockText.implicitWidth+20:modelData==="battery"?50:30
                Accessible.name:modelData==="network"?Radio.wifiStatus:modelData==="bluetooth"?Radio.bluetoothStatus:modelData==="calendar"?"Calendar":modelData==="jared"?"JARED, "+(root.assistant.state||"offline"):modelData==="lock"?"Lock computer":modelData
                ShellToolTip {visible:statusButton.modelData==="lock"&&statusButton.hovered;delay:400;text:"Lock and return to the city lock screen"}
                onClicked:root.showMenu(modelData,statusButton)
                hoverEnabled:true
                TapHandler {enabled:statusButton.modelData==="jared";acceptedButtons:Qt.RightButton;onTapped:Workspace.assistantCommand("preferences","")}
                contentItem:Item {
                    SovelSymbol {anchors.centerIn:parent;anchors.horizontalCenterOffset:statusButton.modelData==="battery"?-9:0;width:16;height:16;visible:statusButton.modelData!=="calendar";symbol:root.iconName(statusButton.modelData);color:SettingsService.colors.text;opacity:statusButton.modelData==="network"&&!Radio.wifiEnabled||statusButton.modelData==="bluetooth"&&!Radio.bluetoothEnabled?.5:1}
                    Text {id:clockText;anchors.centerIn:parent;visible:statusButton.modelData==="calendar";text:root.clock;color:SettingsService.colors.text;font.pixelSize:11}
                    Text {anchors.right:parent.right;anchors.verticalCenter:parent.verticalCenter;visible:statusButton.modelData==="battery";text:root.battery().percent>=0?root.battery().percent+"%":"";color:SettingsService.colors.text;font.pixelSize:9}
                    Rectangle {anchors.right:parent.right;anchors.top:parent.top;width:5;height:5;radius:3;color:SettingsService.colors.accent;visible:statusButton.modelData==="updates"&&Updates.packages.length>0}
                    Rectangle {
                        id:assistantIndicator
                        visible:statusButton.modelData==="jared"
                        anchors.right:parent.right;anchors.bottom:parent.bottom;width:6;height:6;radius:3
                        readonly property string stateName:visible?(root.assistant.state||"offline"):""
                        readonly property bool active:["listening","speaking","thinking","executing"].includes(stateName)
                        color:stateName==="error"?"#fa786d":stateName==="approval"?"#e9b957":stateName==="completed"?"#53ce9f":active?"#5acfe7":stateName==="offline"?SettingsService.colors.muted:SettingsService.colors.accent
                        SequentialAnimation on opacity {
                            running:assistantIndicator.active&&!System.reducedMotion;loops:Animation.Infinite
                            NumberAnimation {to:.38;duration:520}
                            NumberAnimation {to:1;duration:520}
                            onStopped:assistantIndicator.opacity=1
                        }
                    }
                }
            }
        }
    }
    ShellPopup {id:wifiPopover;x:root.popupX;y:30;width:370;height:Math.min(600,sovelWindow.height-46)
        contentItem:ScrollView {id:wifiScroll;clip:true;contentWidth:availableWidth;Loader {width:wifiScroll.availableWidth;active:wifiPopover.visible;sourceComponent:Component {WifiPanel {radio:Radio;onAdvancedRequested:{wifiPopover.close();System.launch("network-config")}}}}}
    }
    ShellPopup {id:bluetoothPopover;x:root.popupX;y:30;width:370;height:Math.min(600,sovelWindow.height-46)
        contentItem:ScrollView {id:bluetoothScroll;clip:true;contentWidth:availableWidth;Loader {width:bluetoothScroll.availableWidth;active:bluetoothPopover.visible;sourceComponent:Component {BluetoothPanel {radio:Radio;onAdvancedRequested:{bluetoothPopover.close();System.launch("bluetooth-config")}}}}}
    }
    ShellPopup {id:soundPopover;x:root.popupX;y:30;width:360;height:Math.min(soundContent.implicitHeight+32,Math.max(160,sovelWindow.height-46))
        contentItem:ScrollView {id:soundScroll;clip:true;contentWidth:availableWidth;Loader {id:soundContent;width:soundScroll.availableWidth;active:soundPopover.visible;sourceComponent:Component {SoundPanel {audio:Audio;compact:true;onFullSettingsRequested:{soundPopover.close();System.launch("sound-settings")}}}}}
    }
    ShellPopup {id:calendarPopover;x:Math.min(root.popupX,root.width-width-8);y:30;width:350;height:calendarContent.implicitHeight+32;contentItem:CalendarPanel {id:calendarContent}}
    ShellPopup {id:jaredMenu;x:Math.max(8,Math.min(root.popupX,root.width-width-8));y:30;width:390;height:Math.min(jaredContent.implicitHeight+32,sovelWindow.height-46)
        property bool startedRecording:false
        onOpened:{Workspace.assistantCommand("connect","");quickJared.forceActiveFocus()}
        Connections {target:jaredMenu;function onClosed(){
            if(jaredMenu.startedRecording){jaredMenu.startedRecording=false;Workspace.assistantCommand("voice-stop","")}
        }}
        Connections {target:Workspace;function onAssistantChanged(){
            if(jaredMenu.visible&&root.assistant.transcript){quickJared.text=root.assistant.transcript;Workspace.assistantCommand("clear-transcript","");quickJared.forceActiveFocus()}
        }}
        contentItem:ScrollView {id:jaredScroll;clip:true;contentWidth:availableWidth
          Column {id:jaredContent;width:jaredScroll.availableWidth;spacing:10
            Row {
                width:parent.width;spacing:10
                Text {width:parent.width-expand.implicitWidth-10;text:"JARED";color:SettingsService.colors.text;font.pixelSize:20;font.bold:true}
                SovelButton {id:expand;text:"Expand";onClicked:{jaredMenu.close();Workspace.assistantCommand("show","")}}
            }
            Text {width:parent.width;text:(root.assistant.state||"offline").toUpperCase()+"  ·  "+(root.assistant.runtime||"Local assistant");color:SettingsService.colors.accent;elide:Text.ElideRight;font.pixelSize:11}
            Text {width:parent.width;text:root.assistant.status||"Connecting to JARED…";color:SettingsService.colors.muted;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:12}
            Text {width:parent.width;visible:!!root.assistant.reply;text:root.assistant.reply||"";color:SettingsService.colors.text;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:13;maximumLineCount:5;elide:Text.ElideRight}
            Text {width:parent.width;visible:!!root.assistant.approval;text:root.assistant.approval||"";color:SettingsService.colors.text;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.bold:true}
            SovelCombo {width:parent.width;visible:(root.assistant.choices||[]).length>1;model:root.assistant.choices||[];textRole:"description";onActivated:Workspace.assistantCommand("choose-output",String(currentIndex))}
            Row {visible:!!root.assistant.approval;spacing:6
                SovelButton {text:"Approve";primary:true;onClicked:Workspace.assistantCommand("approve","")}
                SovelButton {text:"Reject";onClicked:Workspace.assistantCommand("reject","")}
            }
            TextField {id:quickJared;width:parent.width;placeholderText:"Ask JARED…";maximumLength:1000;enabled:!root.assistant.busy&&!root.assistant.recording&&!root.assistant.approval;onAccepted:submitCommand.clicked()}
            Row {spacing:6
                SovelButton {id:submitCommand;text:"Send";primary:true;enabled:quickJared.enabled&&!!quickJared.text.trim()
                    onClicked:{Workspace.assistantCommand("ask",quickJared.text);quickJared.clear()}}
                SovelButton {text:root.assistant.recording?"Stop recording":"Microphone";enabled:!root.assistant.busy&&!root.assistant.approval
                    onClicked:{const record=!root.assistant.recording;jaredMenu.startedRecording=record;Workspace.assistantCommand(record?"voice-start":"voice-stop","")}}
                SovelButton {text:"Cancel";visible:!!root.assistant.busy;onClicked:Workspace.assistantCommand("cancel","")}
            }
          }
        }
    }
    ShellPopup {id:controlPopover;x:root.popupX;y:30;width:370;height:controlContent.implicitHeight+32
        contentItem:Column {id:controlContent;spacing:14
            Text {text:"Control Center";font.pixelSize:19;font.bold:true;color:SettingsService.colors.text}
            Grid {columns:2;spacing:10
                SovelButton {text:Radio.wifiEnabled?"Wi-Fi · On":"Wi-Fi · Off";onClicked:{controlPopover.close();wifiPopover.open()}}
                SovelButton {text:Radio.bluetoothEnabled?"Bluetooth · On":"Bluetooth · Off";onClicked:{controlPopover.close();bluetoothPopover.open()}}
                SovelButton {text:"Sound";onClicked:{controlPopover.close();soundPopover.open()}}
                SovelButton {text:"Displays";onClicked:{controlPopover.close();System.launch("display-settings")}}
            }
            Switch {text:"Do Not Disturb";checked:!!SettingsService.preferences["notifications.focus"];palette.windowText:SettingsService.colors.text;onToggled:SettingsService.setPreference("notifications.focus",checked)}
            Text {text:root.battery().percent>=0?"Battery "+root.battery().percent+"% · "+root.battery().status:"No battery reported";color:SettingsService.colors.muted}
            SovelButton {text:Updates.packages.length?Updates.packages.length+" system updates":"Software Update";onClicked:{controlPopover.close();System.launch("updates")}}
            SovelButton {text:"Settings";onClicked:{controlPopover.close();System.launch("settings")}}
        }
    }
    Timer {interval:15000;repeat:true;running:root.visible;triggeredOnStart:true;onTriggered:{Radio.inspectWifi();Radio.inspectBluetooth()}}
    Timer {interval:1000;running:true;repeat:true;triggeredOnStart:true;onTriggered:root.clock=Qt.formatDateTime(new Date(),(root.preferences["clock.weekday"]?"ddd MMM d   ":"MMM d   ")+(root.preferences["clock.format"]==="24h"?"HH:mm":"h:mm")+(root.preferences["clock.seconds"]?":ss":"")+(root.preferences["clock.format"]==="24h"?"":" AP"))}
}
