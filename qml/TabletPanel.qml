import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Drawing tablets: which display the pen controls, how the cursor follows the
// pen, orientation, buttons, touch and the active area. Labwc maps the pen
// natively, so the cursor jumps to the pen without moving any window.
SettingsCard {
 id:panel
 readonly property var report:SettingsService.reports["tablets"]||({})
 readonly property var config:report.config||({})
 readonly property var outputs:report.outputs||[]
 readonly property var pens:(report.devices||[]).filter(d=>d.kind==="pen")
 readonly property var touches:(report.devices||[]).filter(d=>d.kind==="touch")
 property var draft:({})
 property bool dirty:false
 property string result:""
 function value(key,fallback){return draft[key]!==undefined?draft[key]:config[key]!==undefined?config[key]:fallback}
 function change(key,next){const copy=Object.assign({},draft);copy[key]=next;draft=copy;dirty=true}
 function button(name,fallback){const buttons=value("buttons",{});return buttons[name]||fallback}
 function setButton(name,target){const buttons=Object.assign({},value("buttons",{}));buttons[name]=target;change("buttons",buttons)}
 function save(){
  const payload={output:value("output",""),rotate:value("rotate",0),absolute:value("absolute",true),touch:value("touch",true),buttons:value("buttons",{}),area:value("area",{})}
  SettingsService.perform("tablet-config",payload)
 }
 Component.onCompleted:SettingsService.refresh("tablets",true)
 Connections {target:SettingsService
  function onActionFinished(operation,ok,result){if(operation!=="tablet-config")return;panel.result=ok?"Applied. The pen now follows this mapping.":(result.error||"The tablet settings were not applied.");if(ok){panel.dirty=false;panel.draft={};SettingsService.refresh("tablets",true)}}
 }
 Timer {interval:5000;repeat:true;running:panel.visible&&!panel.dirty;onTriggered:SettingsService.refresh("tablets",true)}

 SettingsLabel {text:"Drawing Tablets";font.bold:true;font.pixelSize:17}
 SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:SettingsService.colors.muted
  text:panel.pens.length?"Connected: "+panel.pens.map(p=>p.name).join(", "):"No pen or drawing tablet is connected. Plug one in and it appears here."}
 // One-time mapping when Sovel cannot tell which screen is the pen display.
 Rectangle {
  Layout.fillWidth:true;visible:!!panel.report.needsMapping&&!panel.dirty;radius:12;implicitHeight:needs.implicitHeight+20
  color:Qt.tint(SettingsService.colors.surface,"#30"+String(SettingsService.colors.accent).slice(1));border.color:SettingsService.colors.accent
  SettingsLabel {id:needs;anchors.fill:parent;anchors.margins:10;wrapMode:Text.WordWrap
   text:"Sovel could not tell which display belongs to your pen display, so the pen controls the whole desktop. Choose its display below once; Sovel remembers it."}
 }
 GridLayout {
  id:form
  Layout.fillWidth:true;columns:width>560?2:1;columnSpacing:14;rowSpacing:10
  SettingsLabel {text:"Pen controls"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var choices:[{id:"",label:"Automatic"+(panel.report.mapped&&!panel.config.output?" ("+((panel.outputs.find(o=>o.name===panel.report.mapped)||{}).description||panel.report.mapped)+")":"")},{id:"*",label:"The whole desktop"}].concat(panel.outputs.map(o=>({id:o.name,label:o.description})))
   model:choices;textRole:"label"
   currentIndex:Math.max(0,choices.findIndex(c=>c.id===panel.value("output","")))
   onActivated:panel.change("output",choices[currentIndex].id)
  }
  SettingsLabel {text:"Cursor follows the pen"}
  RowLayout {Layout.fillWidth:true;Layout.preferredWidth:1;spacing:8
   Switch {id:follow;checked:panel.value("absolute",true);onToggled:panel.change("absolute",checked);Accessible.name:"Cursor follows the pen"}
   SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:SettingsService.colors.muted;text:follow.checked?"The cursor jumps to where the pen touches or hovers.":"The pen moves the cursor like a mouse."}
  }
  SettingsLabel {text:"Orientation"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var values:[0,90,180,270]
   model:["Normal","Rotated 90°","Upside down (left-handed)","Rotated 270°"]
   currentIndex:Math.max(0,values.indexOf(panel.value("rotate",0)))
   onActivated:panel.change("rotate",values[currentIndex])
  }
  SettingsLabel {text:"Lower pen button"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1;readonly property var values:["Right","Middle","Left"]
   model:["Right click","Middle click","Left click"]
   currentIndex:Math.max(0,values.indexOf(panel.button("Stylus","Right")))
   onActivated:panel.setButton("Stylus",values[currentIndex])
  }
  SettingsLabel {text:"Upper pen button"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1;readonly property var values:["Middle","Right","Left"]
   model:["Middle click","Right click","Left click"]
   currentIndex:Math.max(0,values.indexOf(panel.button("Stylus2","Middle")))
   onActivated:panel.setButton("Stylus2",values[currentIndex])
  }
  SettingsLabel {text:"Touch input"}
  RowLayout {Layout.fillWidth:true;Layout.preferredWidth:1;spacing:8
   Switch {id:touchSwitch;checked:panel.value("touch",true);onToggled:panel.change("touch",checked);Accessible.name:"Touch input"}
   SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:SettingsService.colors.muted
    text:panel.touches.length?(touchSwitch.checked?"Touch maps to the same display.":"Touch is not mapped to a display."):"No touch screen detected. Pen-only displays work by hovering and pressing."}
  }
  SettingsLabel {text:"Active area"}
  ColumnLayout {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var area:panel.value("area",{})
   RowLayout {
    CheckBox {id:customArea;text:"Use part of the surface";palette.text:SettingsService.colors.text;checked:!!(parent.parent.area.width&&parent.parent.area.height);palette.windowText:SettingsService.colors.text
     onToggled:panel.change("area",checked?{left:0,top:0,width:120,height:70}:{})}
   }
   Flow {
    Layout.fillWidth:true;visible:customArea.checked;spacing:8
    Repeater {model:[{key:"left",label:"Left"},{key:"top",label:"Top"},{key:"width",label:"Width"},{key:"height",label:"Height"}]
     RowLayout {required property var modelData;spacing:4
      SettingsLabel {text:modelData.label}
      SpinBox {from:0;to:1000;editable:true;value:(panel.value("area",{})[modelData.key]||0)
       onValueModified:{const area=Object.assign({},panel.value("area",{}));area[modelData.key]=value;panel.change("area",area)}}
     }
    }
    SettingsLabel {text:"mm";color:SettingsService.colors.muted}
   }
  }
 }
 RowLayout {
  SovelButton {text:"Apply";primary:true;enabled:panel.dirty&&!SettingsService.busy;onClicked:panel.save()}
  SovelButton {text:"Discard";enabled:panel.dirty;onClicked:{panel.draft={};panel.dirty=false;panel.result=""}}
  SovelButton {text:"Refresh";onClicked:SettingsService.refresh("tablets",true)}
 }
 SettingsLabel {Layout.fillWidth:true;visible:!!panel.result;text:panel.result;wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;color:SettingsService.colors.muted
  text:"The mapping is saved per account and comes back when the tablet is reconnected. The mouse and touchpad keep working normally on every display."}
}
