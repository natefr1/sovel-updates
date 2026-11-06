import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
Item {
 id:root;objectName:"jared-root"
 function showPreferences(){preferences.open()}
 property bool expanded:true
 property bool inspectorOpen:true
 property string page:typeof initialJaredPage!=="undefined"?initialJaredPage:"overview"
 readonly property bool wide:width>=780
 readonly property var routes:[{id:"overview",label:"Command Center"},{id:"chat",label:"Conversations"},{id:"projects",label:"Projects"},{id:"notes",label:"Notes"},{id:"research",label:"Research"},{id:"find",label:"Find Files"},{id:"activity",label:"Activity"}]
 function navigate(target){
  page=target
  if(["notes","projects","research"].includes(page)&&!Jared.busy&&!Jared.pendingAction.type)Jared.work("list",{})
 }
 RowLayout {
  anchors.fill:parent;anchors.margins:14;spacing:14
  Rectangle {
   visible:root.wide;Layout.preferredWidth:160;Layout.fillHeight:true
   radius:14;color:SettingsService.colors.sidebar;border.color:SettingsService.colors.border
   ColumnLayout {
    anchors.fill:parent;anchors.margins:12;spacing:6
    SettingsLabel {text:"WORKSPACE";color:SettingsService.colors.muted;font.pixelSize:10;font.bold:true}
    Repeater {model:root.routes
     SovelButton {required property var modelData;Layout.fillWidth:true;text:modelData.label;primary:root.page===modelData.id;onClicked:root.navigate(modelData.id)}
    }
    Item {Layout.fillHeight:true}
    SovelButton {Layout.fillWidth:true;text:"Models & Settings";onClicked:root.showPreferences()}
   }
  }
 ColumnLayout {
  Layout.fillWidth:true;Layout.fillHeight:true;spacing:12
  RowLayout {Layout.fillWidth:true
   Image {source:"qrc:/assets/icons/jared.svg";Layout.preferredWidth:36;Layout.preferredHeight:36}
   ColumnLayout {Layout.fillWidth:true;spacing:2
    Text {text:"JARED";color:SettingsService.colors.text;font.pixelSize:21;font.bold:true}
    Text {text:Jared.runtimeLabel;color:SettingsService.colors.muted;font.pixelSize:11}
   }
   SovelButton {text:root.expanded?"Compact":"Expand";onClicked:{root.expanded=!root.expanded;sovelWindow.width=Math.min(root.expanded?940:600,sovelWindow.screen.width-40);sovelWindow.height=Math.min(root.expanded?760:650,sovelWindow.screen.height-60)}}
   SovelButton {visible:root.wide;text:root.inspectorOpen?"Hide Inspector":"Inspector";onClicked:root.inspectorOpen=!root.inspectorOpen}
  }
  SovelCombo {visible:!root.wide;Layout.fillWidth:true;model:root.routes;textRole:"label"
   currentIndex:root.routes.findIndex(route=>route.id===root.page)
   onActivated:root.navigate(root.routes[currentIndex].id)
  }
  JaredWorkspaces {Layout.fillWidth:true;Layout.fillHeight:true;visible:["notes","projects","research","find"].includes(root.page);page:root.page;onReturnToChat:root.page="chat";onShowFind:root.page="find"}
  JaredMissionView {Layout.fillWidth:true;Layout.fillHeight:true;visible:["overview","activity"].includes(root.page);activityOnly:root.page==="activity";onNavigate:target=>root.navigate(target)}
  Rectangle {visible:root.page==="chat";Layout.fillWidth:true;Layout.fillHeight:true;radius:14;color:SettingsService.colors.surface;border.color:SettingsService.colors.border
   ListView {id:conversation;anchors.fill:parent;anchors.margins:12;spacing:12;clip:true;model:Jared.messages;onCountChanged:Qt.callLater(()=>positionViewAtEnd())
    delegate:Column {required property var modelData;width:ListView.view.width;spacing:4
     Text {text:modelData.role==="user"?"You":modelData.role==="action"?"System action":"JARED";color:SettingsService.colors.accent;font.pixelSize:11;font.bold:true}
     TextEdit {width:parent.width;text:modelData.text;textFormat:TextEdit.PlainText;readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;color:SettingsService.colors.text;font.pixelSize:14}
    }
    Text {anchors.centerIn:parent;width:parent.width-20;visible:conversation.count===0;text:"Ask a question, open an installed app, change volume, switch audio output or check memory usage. System changes use separate approval controls.";color:SettingsService.colors.muted;wrapMode:Text.Wrap;horizontalAlignment:Text.AlignHCenter;font.pixelSize:14}
    ScrollBar.vertical:ScrollBar {}
   }
  }
  Rectangle {visible:!!Jared.pendingAction.type;Layout.fillWidth:true;implicitHeight:approval.implicitHeight+24;radius:12;color:SettingsService.colors.surfaceHover;border.color:SettingsService.colors.accent
   ColumnLayout {id:approval;anchors.fill:parent;anchors.margins:12;spacing:10
    Text {Layout.fillWidth:true;text:Jared.pendingAction.label||"";textFormat:Text.PlainText;color:SettingsService.colors.text;wrapMode:Text.Wrap;font.bold:true}
    SovelCombo {Layout.fillWidth:true;visible:(Jared.pendingAction.choices||[]).length>1;model:Jared.pendingAction.choices||[];textRole:"description";onActivated:Jared.chooseOutput(currentIndex)}
    RowLayout {SovelButton {text:"Approve";primary:true;onClicked:Jared.approveAction()} SovelButton {text:"Cancel";onClicked:Jared.rejectAction()}}
   }
  }
  ColumnLayout {visible:root.page==="chat"&&!Jared.modelReady;Layout.fillWidth:true;spacing:6
   Text {Layout.fillWidth:true;text:"Download the verified Qwen model once (1.83 GB). No model or microphone runs in the background by default.";color:"#CBB7DA";wrapMode:Text.Wrap;font.pixelSize:12}
   RowLayout {SovelButton {text:"Download local model";enabled:!Jared.busy;onClicked:Jared.downloadModel()} SovelButton {text:"Verify existing model";enabled:!Jared.busy;onClicked:Jared.verifyModel()}}
   ProgressBar {visible:Jared.downloadProgress>=0;Layout.fillWidth:true;from:0;to:100;value:Jared.downloadProgress}
  }
  RowLayout {visible:root.page==="chat";Layout.fillWidth:true
   TextField {id:question;objectName:"jared-question";Layout.fillWidth:true;maximumLength:1000;placeholderText:"Ask JARED…";color:SettingsService.colors.text;placeholderTextColor:SettingsService.colors.muted;background:Rectangle {radius:9;color:SettingsService.colors.field;border.color:question.activeFocus?SettingsService.colors.accent:SettingsService.colors.border} enabled:!Jared.busy&&!Jared.recording&&!Jared.pendingAction.type;selectByMouse:true;onAccepted:send.clicked();Accessible.name:"Message to JARED"}
   SovelButton {text:Jared.recording?"Listening…":"Hold to Talk";enabled:!Jared.busy&&!Jared.pendingAction.type
    onPressed:Jared.recordVoice();onReleased:Jared.stopVoice();onCanceled:Jared.stopVoice()}
   SovelButton {id:send;text:"Send";primary:true;enabled:question.enabled&&question.text.trim().length>0;onClicked:{const text=question.text;question.clear();Jared.ask(text)}}
   SovelButton {text:"Stop";visible:Jared.busy;onClicked:Jared.cancel()}
  }
  Text {Layout.fillWidth:true;text:Jared.status;textFormat:Text.PlainText;color:SettingsService.colors.muted;font.pixelSize:11;wrapMode:Text.Wrap}
  RowLayout {Layout.fillWidth:true
   SovelButton {text:"Preferences";onClicked:preferences.open()}
   SovelButton {text:"Read reply";enabled:!Jared.busy;onClicked:Jared.sayLastReply()}
   SovelButton {text:"Clear chat";enabled:!Jared.busy&&!Jared.pendingAction.type;onClicked:clearConfirmation.open()}
   SovelButton {text:"Unload model";enabled:Jared.loaded&&!Jared.busy;onClicked:Jared.unload()}
  }
 }
 Rectangle {
  visible:root.wide&&root.inspectorOpen;Layout.preferredWidth:205;Layout.fillHeight:true
  radius:14;color:SettingsService.colors.sidebar;border.color:SettingsService.colors.border
  ColumnLayout {
   anchors.fill:parent;anchors.margins:12;spacing:10
   SettingsLabel {text:"INSPECTOR";color:SettingsService.colors.muted;font.pixelSize:10;font.bold:true}
   SettingsLabel {text:"Assistant";font.bold:true}
   SettingsLabel {Layout.fillWidth:true;text:Jared.sessionState.state.toUpperCase();color:SettingsService.colors.accent;wrapMode:Text.Wrap}
   SettingsLabel {Layout.fillWidth:true;text:Jared.runtimeLabel;wrapMode:Text.Wrap}
   SettingsLabel {Layout.fillWidth:true;text:Jared.loaded?"Local model loaded":"No model loaded";color:SettingsService.colors.muted;wrapMode:Text.Wrap}
   SettingsLabel {text:"Workspace";font.bold:true}
   SettingsLabel {Layout.fillWidth:true;text:(Jared.workspaces.projects||[]).length+" loaded projects\n"+(Jared.workspaces.tasks||[]).filter(task=>!task.done).length+" open tasks";wrapMode:Text.Wrap}
   SettingsLabel {Layout.fillWidth:true;text:"Local project data. File and system operations use their existing permission checks.";color:SettingsService.colors.muted;wrapMode:Text.Wrap}
   Item {Layout.fillHeight:true}
  }
 }
 }
 Connections {target:Jared;function onChanged(){if(sovelWindow.visible&&Jared.transcript){root.page="chat";question.text=Jared.transcript;Jared.clearTranscript();question.forceActiveFocus()}}}
 FileDialog {id:whisperPicker;title:"Choose a local Whisper model";nameFilters:["Whisper models (*.bin)"];onAccepted:Jared.setVoiceFile("whisper",selectedFile)}
 FileDialog {id:piperPicker;title:"Choose a local Piper voice";nameFilters:["Piper voices (*.onnx)"];onAccepted:Jared.setVoiceFile("piper",selectedFile)}
 ShellPopup {id:preferences;anchors.centerIn:Overlay.overlay;width:Math.min(460,sovelWindow.width-24);height:Math.min(sovelWindow.height-34,preferencesContent.implicitHeight+32)
  contentItem:ScrollView {id:preferencesScroll;clip:true;contentWidth:availableWidth
   ColumnLayout {id:preferencesContent;width:preferencesScroll.availableWidth;spacing:12
   Text {Layout.fillWidth:true;text:"Local assistant preferences";color:"#F5E9FF";font.pixelSize:18;font.bold:true}
   Switch {text:"Allow basic actions I explicitly request";checked:Jared.allowBasicActions;onToggled:Jared.allowBasicActions=checked;palette.windowText:"#F5E9FF"}
   Text {Layout.fillWidth:true;text:"App launch, volume, output selection and Bluetooth only. Locking still requires approval. Model replies cannot run commands, delete files or grant administrator access.";color:"#CBB7DA";wrapMode:Text.Wrap;font.pixelSize:12}
   Switch {text:"Keep JARED in the background when closed";checked:Jared.keepBackground;onToggled:Jared.keepBackground=checked;palette.windowText:"#F5E9FF"}
   Switch {text:"Read replies aloud";checked:Jared.speakReplies;onToggled:Jared.speakReplies=checked;palette.windowText:"#F5E9FF"}
   Text {text:"Unload idle model after";color:"#CBB7DA"}
   SovelCombo {Layout.fillWidth:true;model:["Keep loaded","1 minute","2 minutes","5 minutes","10 minutes"];property var values:[0,60,120,300,600];currentIndex:Math.max(0,values.indexOf(Jared.idleSeconds));onActivated:Jared.idleSeconds=values[currentIndex]}
   Text {text:"Execution";color:"#CBB7DA"}
   SovelCombo {Layout.fillWidth:true;enabled:!Jared.busy;model:["CPU · conservative default","Automatic · GPU only when supported"];currentIndex:Jared.executionMode==="auto"?1:0;onActivated:Jared.executionMode=currentIndex===1?"auto":"cpu"}
   Text {text:"Local AI model";color:"#CBB7DA"}
   SovelCombo {Layout.fillWidth:true;enabled:!Jared.busy;model:["Automatic · Qwen3.5 4B when ready","Qwen3-1.7B only"];currentIndex:Jared.modelPreference==="auto"?0:1;onActivated:Jared.modelPreference=currentIndex===0?"auto":"light"}
   SovelButton {text:"Download Qwen3.5 4B";enabled:!Jared.busy;onClicked:gpuConsent.open()}
   RowLayout {
    SovelButton {text:"Choose Whisper Model";onClicked:whisperPicker.open()}
    SovelButton {text:"Choose Piper Voice";onClicked:piperPicker.open()}
   }
   SovelButton {text:"Download English Speech Model · 78 MB";enabled:!Jared.busy;onClicked:Jared.work("voice-download",{})}
   SovelButton {text:"Install English Piper Voice · 139 MB";enabled:!Jared.busy;onClicked:piperConsent.open()}
   Text {Layout.fillWidth:true;text:(Jared.whisperModel?"Speech input model selected. ":"Choose a Whisper model to enable push-to-talk. ")+(Jared.piperVoice?"Piper voice selected.":"Speech output uses the system voice until you choose a Piper voice.");color:"#CBB7DA";wrapMode:Text.Wrap;font.pixelSize:12}
   SovelButton {text:"Done";onClicked:preferences.close()}
   }
  }
 }
 Dialog {id:piperConsent;width:380;title:"Install Piper voice?";modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel
  background:ShellGlass {radius:16;tint:.97}
  contentItem:Label {text:"Download the verified Piper runtime and English voice for this account. Speech is generated locally on the CPU.";wrapMode:Text.WordWrap}
  onAccepted:Jared.work("piper-install",{})
 }
 Dialog {id:gpuConsent;width:380;title:"Download Qwen3.5 4B?";modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel
  background:ShellGlass {radius:18;tint:.9}
  contentItem:Text {text:"Ollama downloads about 3.3–4 GB to this computer. JARED checks your GPU driver and free memory before using it.";color:"#F5E9FF";wrapMode:Text.Wrap}
  onAccepted:Jared.downloadGpuModel()
 }
 Dialog {id:clearConfirmation;width:360;title:"Clear local conversation?";modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {radius:18;tint:0.9} contentItem:Text {text:"This clears the chat saved by JARED on this computer.";color:"#F5E9FF";wrapMode:Text.Wrap} onAccepted:Jared.clearHistory()}
}
