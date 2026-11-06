import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
Item {
 id:root;objectName:"settings-root";width:1000;height:680
 property string pageId:typeof initialSettingsPage!=="undefined"&&initialSettingsPage?initialSettingsPage:"home"
 property var history:[]
 property var forwardHistory:[]
 readonly property var pages:SettingsService.pages
 readonly property var selectedPage:pages.find(p=>p.id===root.pageId)||pages[0]||({name:"Settings",id:"home",sections:[]})
 property var actionResult:({})
 property string dialogOperation:""
 property var profileProposal:({})
 function navigateNamed(id){navigate(id)}
 function navigate(id){
  if(id==="wallpaper-image"){photoPicker.open();return}
  if(id==="lockscreen"){id="wallpaper";}
  if(!pages.some(p=>p.id===id))return;
  if(pageId!==id){history=history.concat([pageId]);forwardHistory=[];pageId=id;}else SettingsService.refresh(id)
 }
 function back(){if(history.length){forwardHistory=forwardHistory.concat([pageId]);pageId=history[history.length-1];history=history.slice(0,-1)}}
 function forward(){if(forwardHistory.length){history=history.concat([pageId]);pageId=forwardHistory[forwardHistory.length-1];forwardHistory=forwardHistory.slice(0,-1)}}
 function searchScore(page){const q=search.text.trim().toLowerCase();if(!q)return 1;const name=page.name.toLowerCase();if(name===q||page.id===q)return 100;if(name.includes(q))return 90;let labels=[];for(let s=0;s<page.sections.length;s++)for(let i=0;i<page.sections[s].items.length;i++){const entry=page.sections[s].items[i];if(entry.key)labels.push(String((SettingsService.definitions[entry.key]||{}).label).toLowerCase())}if(labels.includes(q))return 85;if(labels.some(l=>l.includes(q)))return 75;if((page.keywords||[]).some(k=>k.includes(q)))return 60;return JSON.stringify(page).toLowerCase().includes(q)?20:0}
 function matches(page){return searchScore(page)>0}
 function perform(operation){
  if(operation==="refresh"){SettingsService.refresh(pageId,true);return}
  if(operation==="wallpaper-image"){photoPicker.open();return}
  if(operation==="wallpaper-picker"){System.openWallpaperPicker();return}
  if(operation==="lock"){System.launch("lock");return}
  if(["files","library","store","jared","terminal","vault","exit-private"].includes(operation)){System.launch(operation);return}
  if(operation.startsWith("preset-")){
   const outputs=(typeof Displays!=="undefined"&&Displays?Displays.outputs:[]).filter(o=>o.enabled!==false).map(o=>o.name)
   const ok=Workspace.applyPreset(operation.slice(7),outputs,typeof Displays!=="undefined"&&Displays?Displays.primary:"")
   root.actionResult={message:ok?(outputs.length>1?"The layout is on your secondary display. Drag or resize any widget to adjust it.":"Only one display is connected, so the layout was placed on it. It moves to the secondary display when you apply it again with two displays."):"The layout could not be applied.",rows:[]}
   root.dialogOperation=operation;resultDialog.open();return
  }
  if(operation==="export-profile"){profileExport.open();return}
  if(operation==="import-profile"){profileImport.open();return}
  if(operation==="export-diagnostics"){if(!SettingsService.preferences["privacy.diagnostics"]){diagnosticConsent.open();return}SettingsService.refresh("about",true);SettingsService.refresh("developer",true);diagnosticExport.open();return}
  if(operation==="reset-appearance"){resetConfirmation.open();return}
  if(operation==="widget-editor"){widgetDialog.open();return}
  if(operation==="arrange-bar"){barDialog.open();return}
  if(operation==="weather-city"){cityDialog.open();return}
  if(operation==="clear-usage"){usageConfirmation.open();return}
  if(operation==="account-picture"){avatarPicker.open();return}
  if(operation==="choose-locale"||operation==="default-apps"||operation==="startup-items"){dialogOperation=operation;SettingsService.perform(operation);return}
  if(["hostname","timezone","network-time","display-name","firewall-review"].includes(operation)){dialogOperation=operation;editValue.text="";editDialog.open();return}
  SettingsService.perform(operation)
 }
 onPageIdChanged:{SettingsService.visit(pageId);SettingsService.refresh(pageId);if(contentScroll.contentItem)contentScroll.contentItem.contentY=0;if(!System.reducedMotion)arrival.restart()}
 Dialog {id:barDialog;title:"Arrange menu bar icons";width:440;height:Math.min(640,sovelWindow.height-40);modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Close;background:ShellGlass {tint:.95} contentItem:ScrollView {id:arrangeScroll;clip:true;contentWidth:availableWidth;BarArrangement {width:arrangeScroll.availableWidth}}}
 Component.onCompleted:{if(!pages.some(p=>p.id===pageId))pageId="home";SettingsService.visit(pageId);SettingsService.refresh(pageId)}
 Shortcut {sequence:"Ctrl+F";onActivated:{search.forceActiveFocus();search.selectAll()}}
 Shortcut {sequence:"Alt+Left";onActivated:root.back()}
 Shortcut {sequence:"Alt+Right";onActivated:root.forward()}
 Rectangle {anchors.fill:parent;color:SettingsService.colors.background}
 Rectangle {id:sidebar;width:root.width<790?210:244;height:parent.height;color:SettingsService.colors.sidebar
  Rectangle {anchors.right:parent.right;height:parent.height;width:1;color:SettingsService.colors.border;opacity:.4}
  ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:14
   RowLayout {Layout.fillWidth:true;spacing:11
    Rectangle {width:38;height:38;radius:19;color:SettingsService.colors.accent
     Text {anchors.centerIn:parent;text:System.username.slice(0,1).toUpperCase();font.pixelSize:18;font.bold:true;color:SettingsService.colors.accentText;visible:accountPhoto.status!==Image.Ready}
     Image {id:accountPhoto;anchors.fill:parent;source:SettingsService.avatarUrl;fillMode:Image.PreserveAspectCrop;sourceSize:Qt.size(76,76)}
    }
    ColumnLayout {Layout.fillWidth:true;spacing:2;SettingsLabel {text:System.username;font.bold:true} SettingsLabel {text:"Local account";font.pixelSize:11;color:SettingsService.colors.muted}}
   }
   TextField {id:search;objectName:"settings-search";Layout.fillWidth:true;placeholderText:"Search Settings";color:SettingsService.colors.text;placeholderTextColor:SettingsService.colors.muted;selectByMouse:true
    background:Rectangle {radius:10;color:SettingsService.colors.field;border.color:search.activeFocus?SettingsService.colors.accent:SettingsService.colors.border}
    onAccepted:{const ranked=pages.map(p=>({page:p,score:root.searchScore(p)})).filter(r=>r.score>0).sort((a,b)=>b.score-a.score);if(ranked.length)root.navigate(ranked[0].page.id)}
    Keys.onEscapePressed:text=""
   }
   ScrollView {id:sideScroll;objectName:"settings-sections";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;contentWidth:availableWidth
    Column {width:sideScroll.availableWidth;spacing:3
     Repeater {model:root.pages
      Column {id:navItem;required property var modelData;required property int index;width:parent.width;visible:root.matches(modelData);height:visible?implicitHeight:0
       Text {width:parent.width;height:visible?25:0;visible:!search.text.trim()&&(navItem.index===0||root.pages[navItem.index-1].group!==navItem.modelData.group);text:navItem.modelData.group.toUpperCase();leftPadding:9;topPadding:9;color:SettingsService.colors.muted;font.pixelSize:9;font.letterSpacing:1}
       Button {id:navButton;objectName:"settings-page-"+navItem.modelData.id;width:parent.width;height:SettingsService.preferences["appearance.density"]==="compact"?32:38;padding:8;hoverEnabled:true;Accessible.name:navItem.modelData.name
        background:Rectangle {radius:10;color:root.pageId===navItem.modelData.id?SettingsService.colors.accent:navButton.hovered?SettingsService.colors.surfaceHover:"transparent"}
        contentItem:RowLayout {spacing:11
         SovelSymbol {symbol:navItem.modelData.icon;color:root.pageId===navItem.modelData.id?SettingsService.colors.accentText:SettingsService.colors.text;Layout.preferredWidth:19;Layout.preferredHeight:19}
         Text {Layout.fillWidth:true;text:navItem.modelData.name;color:root.pageId===navItem.modelData.id?SettingsService.colors.accentText:SettingsService.colors.text;font.pixelSize:12;elide:Text.ElideRight}
         Text {visible:(SettingsService.preferences.favorites||[]).includes(navItem.modelData.id);text:"★";color:root.pageId===navItem.modelData.id?SettingsService.colors.accentText:SettingsService.colors.accent;font.pixelSize:10}
        }
        onClicked:root.navigate(navItem.modelData.id)
       }
      }
     }
     SettingsLabel {visible:root.pages.every(p=>!root.matches(p));text:"No matching settings";color:SettingsService.colors.muted}
    }
   }
   SettingsLabel {text:"Sovel Desktop";font.pixelSize:10;color:SettingsService.colors.muted}
  }
 }
 ColumnLayout {anchors.left:sidebar.right;anchors.right:parent.right;anchors.top:parent.top;anchors.bottom:parent.bottom;anchors.margins:24;spacing:18
  RowLayout {Layout.fillWidth:true;spacing:9
   SovelButton {text:"‹";implicitWidth:32;implicitHeight:32;enabled:root.history.length>0;onClicked:root.back();Accessible.name:"Back"}
   SovelButton {text:"›";implicitWidth:32;implicitHeight:32;enabled:root.forwardHistory.length>0;onClicked:root.forward();Accessible.name:"Forward"}
   ColumnLayout {Layout.fillWidth:true;spacing:3
    SettingsLabel {text:root.selectedPage.group||"";font.pixelSize:10;color:SettingsService.colors.muted}
    SettingsLabel {text:root.selectedPage.name;font.pixelSize:26;font.weight:Font.DemiBold}
   }
   BusyIndicator {implicitWidth:24;implicitHeight:24;visible:SettingsService.busy;running:visible}
   SovelButton {text:(SettingsService.preferences.favorites||[]).includes(root.pageId)?"★":"☆";implicitWidth:34;implicitHeight:34;Accessible.name:"Favorite this page";onClicked:SettingsService.favorite(root.pageId,!(SettingsService.preferences.favorites||[]).includes(root.pageId))}
  }
  ScrollView {id:contentScroll;objectName:"settings-content";Layout.fillWidth:true;Layout.fillHeight:true;contentWidth:availableWidth;clip:true
   ColumnLayout {id:content;width:contentScroll.availableWidth;spacing:20
    SettingsLabel {text:root.selectedPage.description||"";color:SettingsService.colors.muted;lineHeight:1.3}
    SettingsPage {page:root.selectedPage;Layout.fillWidth:true;onNavigate:target=>root.navigate(target);onAction:operation=>root.perform(operation)}
    SettingsLabel {visible:!!SettingsService.status;text:SettingsService.status;color:SettingsService.colors.muted;font.pixelSize:11}
   }
  }
 }
 NumberAnimation {id:arrival;target:content;property:"opacity";from:.3;to:1;duration:160;easing.type:Easing.OutCubic}
 Connections {target:SettingsService;function onActionFinished(operation,success,result){if(!success){root.actionResult={rows:[{label:"Change could not be applied",value:result.error||"Try again."}]};resultDialog.open();return} if(result.rows||result.history||result.locales||result.associations||result.startup){root.actionResult=result;root.dialogOperation=operation;resultDialog.open()}}}
 FileDialog {id:photoPicker;title:"Choose a background image";nameFilters:["Images (*.png *.jpg *.jpeg *.webp *.bmp *.tif *.tiff)","All files (*)"];onAccepted:System.setWallpaperFile(selectedFile)}
 FileDialog {id:avatarPicker;title:"Choose your local account photo";nameFilters:["Images (*.png *.jpg *.jpeg *.webp)"];onAccepted:if(!SettingsService.setAvatar(selectedFile))System.message("That account image could not be saved. Choose a supported local image up to 32 megapixels.")}
 Dialog {id:diagnosticConsent;title:"Allow a local diagnostic export?";width:360;height:240;modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {tint:.95}
  contentItem:Text {text:"The export contains computer, display, power and developer-service information. It excludes Wi-Fi credentials, network addresses and chat history. You choose where to save it; nothing is uploaded.";wrapMode:Text.Wrap;color:SettingsService.colors.text}
  onAccepted:{SettingsService.setPreference("privacy.diagnostics",true);root.perform("export-diagnostics")}
 }
 FileDialog {id:profileExport;title:"Export appearance profile";fileMode:FileDialog.SaveFile;nameFilters:["Sovel profile (*.json)"];onAccepted:SettingsService.exportProfile(selectedFile)}
 FileDialog {id:profileImport;title:"Choose an appearance profile";nameFilters:["Sovel profile (*.json)"];onAccepted:{root.profileProposal=SettingsService.readProfile(selectedFile);profileConfirmation.open()}}
 FileDialog {id:diagnosticExport;title:"Export diagnostic summary";fileMode:FileDialog.SaveFile;nameFilters:["Diagnostic summary (*.json)"];onAccepted:SettingsService.exportDiagnostics(selectedFile)}
 Dialog {id:profileConfirmation;title:"Review appearance profile";width:Math.min(440,root.width-40);modal:true;anchors.centerIn:Overlay.overlay;standardButtons:root.profileProposal.error?Dialog.Close:Dialog.Apply|Dialog.Cancel;background:ShellGlass {radius:18;tint:.9}
  contentItem:Text {text:root.profileProposal.error||"Apply "+root.profileProposal.count+" appearance preferences? This cannot change system services or grant JARED permissions.";color:SettingsService.colors.text;wrapMode:Text.Wrap}
  onApplied:{SettingsService.applyProfile(root.profileProposal);close()}
 }
 Dialog {id:resetConfirmation;title:"Reset appearance?";width:360;modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {radius:18;tint:.9} contentItem:Text {text:"Restore Sovel's default colors, density, text and motion preferences?";color:SettingsService.colors.text;wrapMode:Text.Wrap} onAccepted:SettingsService.resetAppearance()}
 Dialog {id:widgetDialog;title:"Desktop widgets";width:Math.min(740,root.width-32);height:Math.min(620,root.height-32);modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Close;background:ShellGlass {tint:.95}
  contentItem:Loader {active:widgetDialog.visible;sourceComponent:Component {WidgetManager {}}}
 }
 Dialog {id:cityDialog;title:"Choose your weather city";width:Math.min(500,root.width-32);height:Math.min(500,root.height-32);modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Close;background:ShellGlass {tint:.95}
  contentItem:ColumnLayout {spacing:12
   SettingsLabel {text:"Weather requests go to Open-Meteo only after you enable access. City search uses Open-Meteo and GeoNames; automatic location is never requested.";font.pixelSize:12}
   SettingsToggle {checked:SettingsService.preferences["weather.enabled"];Accessible.name:"Allow weather network access";onClicked:SettingsService.setPreference("weather.enabled",checked)}
   RowLayout {Layout.fillWidth:true
    TextField {id:cityQuery;Layout.fillWidth:true;placeholderText:"City or postal code";color:SettingsService.colors.text;maximumLength:100;onAccepted:Workspace.searchCity(text)}
    SovelButton {text:"Search";enabled:SettingsService.preferences["weather.enabled"];onClicked:Workspace.searchCity(cityQuery.text)}
   }
   ListView {Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:6;model:Workspace.cities
    delegate:SovelButton {required property var modelData;required property int index;width:ListView.view.width;text:[modelData.name,modelData.region,modelData.country].filter(v=>v).join(", ");onClicked:{Workspace.chooseCity(index);cityDialog.close()}}
    ScrollBar.vertical:ScrollBar {}
   }
   SettingsLabel {text:Workspace.status;font.pixelSize:11;color:SettingsService.colors.muted}
  }
 }
 Dialog {id:usageConfirmation;title:"Clear local usage history?";width:360;height:220;modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {tint:.95}
  contentItem:Text {text:"Delete the foreground-app time recorded by Sovel on this account? Application data is kept.";wrapMode:Text.Wrap;color:SettingsService.colors.text}
  onAccepted:Workspace.clearUsage()
 }
 Dialog {id:resultDialog;title:root.dialogOperation==="default-apps"?"Default applications":root.dialogOperation==="startup-items"?"Login items":root.dialogOperation==="choose-locale"?"Display language":"Settings details";width:Math.min(550,root.width-32);height:Math.min(490,root.height-32);modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Close;background:ShellGlass {radius:18;tint:.9}
  contentItem:ScrollView {id:resultsScroll;clip:true;contentWidth:availableWidth
   ColumnLayout {width:resultsScroll.availableWidth;spacing:16
    Text {Layout.fillWidth:true;text:root.actionResult.message||"";color:SettingsService.colors.muted;wrapMode:Text.Wrap}
    Repeater {model:root.actionResult.rows||[];ColumnLayout {required property var modelData;Layout.fillWidth:true;Text {text:modelData.label;color:SettingsService.colors.text;font.bold:true} Text {Layout.fillWidth:true;text:String(modelData.value||"");color:SettingsService.colors.muted;wrapMode:Text.Wrap;textFormat:Text.PlainText}}}
    Repeater {model:root.actionResult.associations||[];ColumnLayout {required property var modelData;Layout.fillWidth:true;spacing:6
     Text {text:modelData.name;color:SettingsService.colors.text;font.bold:true}
     SovelCombo {Layout.fillWidth:true;model:modelData.apps;textRole:"name";currentIndex:modelData.apps.findIndex(a=>a.id===modelData.current);enabled:count>0;onActivated:SettingsService.perform("set-default-app",{mime:modelData.mime,app:modelData.apps[currentIndex].id})}
     Text {visible:!modelData.apps.length;text:"No installed application advertises support for this type.";color:SettingsService.colors.muted;wrapMode:Text.Wrap}
    }}
    Repeater {model:root.actionResult.startup||[];RowLayout {required property var modelData;Layout.fillWidth:true;Text {Layout.fillWidth:true;text:modelData.name;elide:Text.ElideRight;color:SettingsService.colors.text} SettingsToggle {checked:modelData.enabled;Accessible.name:"Open "+modelData.name+" at sign-in";onClicked:SettingsService.perform("set-startup",{app:modelData.id,enabled:checked})}}}
    Repeater {model:root.actionResult.locales||[];SovelButton {required property string modelData;Layout.fillWidth:true;text:modelData;onClicked:{SettingsService.perform("set-locale",{locale:modelData});resultDialog.close()}}}
    Repeater {model:root.actionResult.history?.data||[];ColumnLayout {required property var modelData;Layout.fillWidth:true;Text {Layout.fillWidth:true;text:modelData.summary?.data||"Notification";color:SettingsService.colors.text;wrapMode:Text.Wrap;textFormat:Text.PlainText} Text {Layout.fillWidth:true;text:modelData.body?.data||"";color:SettingsService.colors.muted;wrapMode:Text.Wrap;textFormat:Text.PlainText}}}
   }
  }
 }
 Dialog {id:editDialog;title:root.dialogOperation==="hostname"?"Rename computer":root.dialogOperation==="timezone"?"Time zone":root.dialogOperation==="display-name"?"Your display name":root.dialogOperation==="network-time"?"Network time":"Firewall";width:Math.min(460,root.width-32);modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {radius:18;tint:.9}
  contentItem:ColumnLayout {spacing:12
   Text {Layout.fillWidth:true;text:"The system will request administrator approval before applying this change.";color:SettingsService.colors.muted;wrapMode:Text.Wrap}
   TextField {id:editValue;Layout.fillWidth:true;visible:["hostname","timezone","display-name"].includes(root.dialogOperation);placeholderText:root.dialogOperation==="timezone"?"Example: America/Chicago":"New name";maximumLength:80;selectByMouse:true;color:SettingsService.colors.text;background:Rectangle {radius:9;color:"#24182F";border.color:"#785A91"}}
   Switch {id:enabledValue;visible:root.dialogOperation==="network-time"||root.dialogOperation==="firewall-review";text:root.dialogOperation==="network-time"?"Enable network time":"Enable firewall";checked:true;palette.windowText:"#F5EDFF"}
   Switch {id:keepSSH;visible:root.dialogOperation==="firewall-review";text:"Keep OpenSSH access allowed";checked:true;palette.windowText:"#F5EDFF"}
  }
  onAccepted:{if(root.dialogOperation==="hostname")SettingsService.perform("set-hostname",{name:editValue.text});else if(root.dialogOperation==="timezone")SettingsService.perform("set-timezone",{zone:editValue.text});else if(root.dialogOperation==="display-name")SettingsService.perform("set-full-name",{name:editValue.text});else if(root.dialogOperation==="network-time")SettingsService.perform("set-ntp",{enabled:enabledValue.checked});else SettingsService.perform("set-firewall",{enabled:enabledValue.checked,keepSSH:keepSSH.checked})}
 }
}
