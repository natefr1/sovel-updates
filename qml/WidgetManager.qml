import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
 id:root;spacing:14
 property string section:"library"
 property string group:"All"
 RowLayout {Layout.fillWidth:true
  SovelButton {text:"Widget library";primary:root.section==="library";onClicked:root.section="library"}
  SovelButton {text:"On your desktop · "+Workspace.layout.length;primary:root.section==="desktop";onClicked:root.section="desktop"}
 }
 SettingsLabel {text:root.section==="library"?"Add a widget, then right-click the desktop to arrange it and drag its edges to resize.":"Choose a default size, transparency and refresh interval. Custom sizes are saved for each display.";color:SettingsService.colors.muted}
 SovelCombo {Layout.fillWidth:true;visible:root.section==="library";model:["All","Productivity","System","Information","Media & Sovel"];onActivated:root.group=currentText}
 ListView {id:library;visible:root.section==="library";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:8
  model:Workspace.registry.filter(w=>root.group==="All"||w.group===root.group)
  delegate:Rectangle {required property var modelData;width:library.width;height:74;radius:12;color:SettingsService.colors.surface;border.color:SettingsService.colors.border
   RowLayout {anchors.fill:parent;anchors.margins:14;spacing:13
    SovelSymbol {symbol:modelData.icon;color:SettingsService.colors.accent;Layout.preferredWidth:26;Layout.preferredHeight:26}
    ColumnLayout {Layout.fillWidth:true;spacing:3
     SettingsLabel {text:modelData.name;font.bold:true}
     SettingsLabel {text:modelData.network?"Uses your chosen city · network permission required":modelData.group;font.pixelSize:11;color:SettingsService.colors.muted}
    }
    SovelButton {text:"Add";onClicked:{const id=Workspace.addWidget(modelData.type);if(id)System.message(modelData.name+" added to your desktop.")}}
   }
  }
  ScrollBar.vertical:ScrollBar {}
 }
 ListView {id:installed;visible:root.section==="desktop";Layout.fillWidth:true;Layout.fillHeight:true;clip:true;spacing:10;model:Workspace.layout
  delegate:Rectangle {id:row;required property var modelData;width:installed.width;height:176;radius:12;color:SettingsService.colors.surface;border.color:SettingsService.colors.border
   ColumnLayout {anchors.fill:parent;anchors.margins:14;spacing:10
    RowLayout {Layout.fillWidth:true
     SettingsLabel {text:(Workspace.registry.find(w=>w.type===row.modelData.type)||{}).name||row.modelData.type;font.bold:true}
     SovelButton {text:"Remove…";implicitHeight:30;onClicked:{removal.widget=row.modelData;removal.open()}}
    }
    RowLayout {Layout.fillWidth:true
     SettingsLabel {text:"Default size"}
     SovelCombo {Layout.preferredWidth:150;model:["Small","Medium","Large"];currentIndex:["small","medium","large"].indexOf(row.modelData.size);onActivated:Workspace.configureWidget(row.modelData.id,["small","medium","large"][currentIndex],row.modelData.opacity,row.modelData.refresh)}
     SettingsLabel {text:"Refresh";Layout.fillWidth:false}
     SovelCombo {Layout.preferredWidth:150;property var choices:row.modelData.type==="weather"?[300,900,1800,3600]:row.modelData.type==="gpu"?[10,30,60]:[1,2,10,30,60];model:choices.map(n=>n>=60?(n/60)+" min":n+" sec");currentIndex:Math.max(0,choices.indexOf(row.modelData.refresh));onActivated:Workspace.configureWidget(row.modelData.id,row.modelData.size,row.modelData.opacity,choices[currentIndex])}
    }
    RowLayout {Layout.fillWidth:true
     SettingsLabel {text:"Transparency";Layout.fillWidth:false;Layout.preferredWidth:100}
     Slider {id:opacitySlider;Layout.fillWidth:true;from:25;to:100;stepSize:1;value:row.modelData.opacity;property real requested:row.modelData.opacity;property bool dirty:false;onMoved:{requested=value;dirty=true;if(!pressed){Workspace.configureWidget(row.modelData.id,row.modelData.size,Math.round(value),row.modelData.refresh);dirty=false}}onPressedChanged:if(!pressed&&dirty){dirty=false;Workspace.configureWidget(row.modelData.id,row.modelData.size,Math.round(requested),row.modelData.refresh)} Accessible.name:"Widget opacity"}
     SettingsLabel {Layout.fillWidth:false;text:Math.round(opacitySlider.value)+"% opacity";font.pixelSize:11}
    }
    SettingsLabel {text:"Position: "+Math.round(row.modelData.x*100)+"%, "+Math.round(row.modelData.y*100)+"% · Reposition by dragging in desktop edit mode.";font.pixelSize:10;color:SettingsService.colors.muted}
   }
  }
  ScrollBar.vertical:ScrollBar {}
 }
 SettingsLabel {visible:root.section==="desktop"&&!Workspace.layout.length;text:"Your desktop has no widgets. Add one from the library.";color:SettingsService.colors.muted}
 SettingsLabel {text:Workspace.status;visible:!!Workspace.status;color:SettingsService.colors.muted;font.pixelSize:11}
 Dialog {id:removal;property var widget:({});title:"Remove this widget?";width:340;height:210;modal:true;anchors.centerIn:Overlay.overlay;standardButtons:Dialog.Ok|Dialog.Cancel;background:ShellGlass {tint:.9}
  contentItem:Text {text:"Remove this widget and its local content from the desktop?";wrapMode:Text.Wrap;color:SettingsService.colors.text}
  onAccepted:Workspace.removeWidget(widget.id)
 }
}
