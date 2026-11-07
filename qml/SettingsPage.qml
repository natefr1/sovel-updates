import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
 id:root
 required property var page
 property var report:SettingsService.reports[page.id]||({})
 signal navigate(string target)
 signal action(string operation)
 Layout.fillWidth:true;spacing:18
 function available(key){return !key||!!((report.capabilities||{})[key]||{}).available}
 RowLayout {visible:root.page.id==="home";Layout.fillWidth:true;spacing:12
  Repeater {model:[{label:"CPU",value:Number(System.metrics.cpu)>=0?Number(System.metrics.cpu).toFixed(0)+"%":"Measuring…"},{label:"Memory",value:Number(System.metrics.ramUsed||0).toFixed(1)+" GB"},{label:"Available",value:Number(System.metrics.diskFree||0).toFixed(1)+" GB"}]
   SettingsCard {required property var modelData;Layout.fillWidth:true
    SettingsLabel {text:modelData.label;color:SettingsService.colors.muted;font.pixelSize:11}
    SettingsLabel {text:modelData.value;font.pixelSize:24;font.weight:Font.DemiBold}
   }
  }
 }
 Loader {id:livePanel;Layout.fillWidth:true;Layout.preferredHeight:item?item.implicitHeight:0
  sourceComponent:root.page.id==="sound"?soundPanel:root.page.id==="devices"?bluetoothPanel:root.page.id==="network"?networkPanel:root.page.id==="power"?powerPanel:root.page.id==="wallpaper"?wallpaperPanel:root.page.id==="updates"?updatesPanel:root.page.id==="displays"&&typeof Displays!=="undefined"&&Displays?displayPanel:null
 }
 Component {id:soundPanel;SoundPanel {audio:Audio}}
 Component {id:displayPanel;DisplayPanel {}}
 Component {id:bluetoothPanel;BluetoothPanel {radio:Radio;onAdvancedRequested:System.launch("bluetooth-config")}}
 Component {id:networkPanel;ColumnLayout {spacing:16
  Component.onCompleted:System.refreshNetwork()
  WifiPanel {Layout.fillWidth:true;radio:Radio;onAdvancedRequested:System.launch("network-config")}
  SettingsCard {SettingsLabel {text:"Connection details";font.bold:true} TextEdit {Layout.fillWidth:true;Layout.preferredHeight:contentHeight;text:System.networkReport;readOnly:true;selectByMouse:true;wrapMode:TextEdit.Wrap;color:SettingsService.colors.muted;font.pixelSize:12}}
 }}
 Component {id:powerPanel;SettingsCard {
  Component.onCompleted:System.refreshPower()
  SettingsLabel {text:"Power profile";font.bold:true}
  SettingsLabel {text:System.power.available?"Choose a profile reported by this device.":(System.power.error||"Checking the power service…");color:SettingsService.colors.muted}
  Repeater {model:System.power.profiles||[];RadioButton {required property string modelData;text:modelData==="balanced"?"Balanced":modelData==="power-saver"?"Power Saver":"Performance";checked:System.power.current===modelData;onClicked:System.setPowerProfile(modelData);palette.windowText:SettingsService.colors.text}}
 }}
 Component {id:wallpaperPanel;SettingsCard {
  Image {Layout.fillWidth:true;Layout.preferredHeight:190;source:System.wallpaperUrl;fillMode:Image.PreserveAspectCrop;clip:true}
  RowLayout {SovelButton {text:"Choose image…";primary:true;onClicked:root.action("wallpaper-image")} SovelButton {text:"Wallpaper collection…";onClicked:System.openWallpaperPicker()}}
  Flow {Layout.fillWidth:true;spacing:9
   Repeater {model:System.privateMode?[{key:"emblem",name:"Spider emblem"},{key:"lab",name:"Laboratory"},{key:"landscape",name:"Violet landscape"},{key:"city",name:"City"}]:[{key:"city",name:"City"},{key:"lockroom",name:"Sovel room"}];SovelButton {required property var modelData;text:modelData.name;primary:System.wallpaper===modelData.key;onClicked:System.wallpaper=modelData.key}}
  }
 }}
 Component {id:updatesPanel;UpdatesPanel {}}
 SettingsCard {visible:(root.report.rows||[]).length>0
  RowLayout {SettingsLabel {text:root.page.id==="home"?"This computer":"Current status";font.bold:true} SovelButton {text:"Refresh";implicitHeight:30;onClicked:SettingsService.refresh(root.page.id,true)}}
  Repeater {model:root.report.rows||[]
   RowLayout {required property var modelData;Layout.fillWidth:true;spacing:18
    SettingsLabel {Layout.preferredWidth:145;Layout.fillWidth:false;text:modelData.label;color:SettingsService.colors.muted;verticalAlignment:Text.AlignTop}
    SettingsLabel {text:modelData.value}
   }
  }
 }
 SettingsCard {visible:!!root.report.error
  SettingsLabel {text:"Status could not be refreshed";font.bold:true}
  SettingsLabel {text:root.report.error||"";color:SettingsService.colors.muted}
  SovelButton {text:"Try again";onClicked:SettingsService.refresh(root.page.id,true)}
 }
 SettingsCard {visible:root.page.id==="screentime"
  SettingsLabel {text:"Local foreground-app history";font.bold:true}
  SettingsLabel {text:"Opt-in foreground time includes idle time while an app remains focused. No document titles, browsing URLs or conversation contents are recorded.";color:SettingsService.colors.muted;font.pixelSize:12}
  Repeater {model:Workspace.usage;RowLayout {required property var modelData;Layout.fillWidth:true;SettingsLabel {text:modelData.app;elide:Text.ElideRight}SettingsLabel {Layout.fillWidth:false;text:modelData.day+" · "+Math.round(Number(modelData.seconds)/60)+" min";color:SettingsService.colors.muted;font.pixelSize:11}}}
  SettingsLabel {visible:!Workspace.usage.length;text:"No local usage history recorded.";color:SettingsService.colors.muted}
 }
 Repeater {model:root.page.sections||[]
  SettingsCard {required property var modelData
   SettingsLabel {text:modelData.title;font.pixelSize:Number(SettingsService.preferences["appearance.textSize"]||13)+3;font.weight:Font.DemiBold}
   Repeater {model:modelData.items
    Loader {required property var modelData;Layout.fillWidth:true;property var entry:modelData;sourceComponent:entry.type==="preference"?preferenceRow:entry.type==="support"?supportRow:entry.type==="info"?infoRow:actionRow
     Component {id:preferenceRow;ColumnLayout {spacing:6
      SettingsPreference {preferenceKey:entry.key;supported:root.available((SettingsService.definitions[entry.key]||{}).capability||"")}
      SettingsLabel {visible:!root.available((SettingsService.definitions[entry.key]||{}).capability||"");text:((root.report.capabilities||{})[(SettingsService.definitions[entry.key]||{}).capability]||{}).reason||"This device does not expose this feature.";color:SettingsService.colors.muted;font.pixelSize:11}
     }}
     Component {id:infoRow;ColumnLayout {spacing:5;SettingsLabel {text:entry.label;font.weight:Font.DemiBold} SettingsLabel {text:entry.detail;color:SettingsService.colors.muted}}}
     Component {id:supportRow;ColumnLayout {spacing:5
      SettingsLabel {text:entry.label;font.weight:Font.DemiBold}
      SettingsLabel {text:((root.report.capabilities||{})[entry.capability]||{}).reason||entry.detail;color:SettingsService.colors.muted}
     }}
     Component {id:actionRow;Button {id:actionButton;Layout.fillWidth:true;implicitHeight:entry.detail?64:42;hoverEnabled:true;enabled:entry.type==="link"||root.available(entry.capability||"");Accessible.name:entry.label
      background:Rectangle {radius:9;color:actionButton.hovered?SettingsService.colors.surfaceHover:"transparent";border.color:actionButton.activeFocus?SettingsService.colors.accent:"transparent"}
      contentItem:RowLayout {spacing:12
       ColumnLayout {Layout.fillWidth:true;spacing:4
        SettingsLabel {text:entry.label;opacity:actionButton.enabled?1:.55}
        SettingsLabel {visible:!!entry.detail||!actionButton.enabled;text:actionButton.enabled?entry.detail:((root.report.capabilities||{})[entry.capability]||{}).reason||"Not available in this session.";font.pixelSize:11;color:SettingsService.colors.muted}
       }
       Text {text:entry.type==="link"?"›":"↗";color:SettingsService.colors.muted;font.pixelSize:20}
      }
      onClicked:if(entry.type==="link")root.navigate(entry.target);else root.action(entry.operation)
     }}
    }
   }
  }
 }
 Repeater {model:[{key:"usb",title:"Connected USB devices"},{key:"graphics",title:"Graphics adapters"},{key:"batteries",title:"Battery and UPS"},{key:"volumes",title:"Disks & volumes"}]
  SettingsCard {required property var modelData;property var section:modelData;visible:(root.report[section.key]||[]).length>0
   SettingsLabel {text:section.title;font.bold:true}
   Repeater {model:root.report[section.key]||[];ColumnLayout {required property var modelData;Layout.fillWidth:true;spacing:4
    SettingsLabel {text:modelData.model||modelData.name||modelData.label||modelData.id||"Device";font.weight:Font.DemiBold}
    SettingsLabel {text:Object.keys(modelData).filter(k=>k!=="children"&&modelData[k]!==null&&modelData[k]!=="").map(k=>k+": "+(Array.isArray(modelData[k])?modelData[k].join(", "):modelData[k])).join(" · ");font.pixelSize:11;color:SettingsService.colors.muted}
   }}
  }
 }
}
