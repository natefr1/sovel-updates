import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

SettingsCard {
 id: panel
 property var draft: []
 property string selected: ""
 property string primary: ""
 property string barPlacement: "all"
 property string dockPlacement: "all"
 property var wallpapers: ({})
 property bool dirty: false
 property string notice:""
 readonly property var active: draft.find(output=>output.name===selected) || null
 readonly property real mapScale: Math.min((arrangement.width-32)/Math.max(1,bounds.right-bounds.left),
                                          (arrangement.height-32)/Math.max(1,bounds.bottom-bounds.top),0.20)
 property var bounds: ({left:0,top:0,right:1920,bottom:1080})

 function modeOf(output) {
  return output.modes.find(mode=>mode.id===output.mode) || {width:1280,height:720}
 }
 function extent(output) {
  const mode=modeOf(output)
  const rotated=output.transform.endsWith("90")||output.transform.endsWith("270")
  return {width:Math.round((rotated?mode.height:mode.width)/output.scale),
          height:Math.round((rotated?mode.width:mode.height)/output.scale)}
 }
 function fit() {
  if(!draft.length)return
  bounds={left:Math.min(...draft.map(o=>o.x)),top:Math.min(...draft.map(o=>o.y)),
          right:Math.max(...draft.map(o=>o.x+extent(o).width)),
          bottom:Math.max(...draft.map(o=>o.y+extent(o).height))}
 }
 function sync() {
  if(dirty||Displays.pending||Displays.busy)return
  draft=Displays.outputs.map(o=>Object.assign({},o))
  if(!draft.some(o=>o.name===selected))selected=draft.length?draft[0].name:""
  primary=Displays.primary
  if(!draft.some(o=>o.name===primary&&o.enabled))primary=(draft.find(o=>o.enabled)||{}).name||""
  barPlacement=Displays.config.bar||"all"
  dockPlacement=Displays.config.dock||"all"
  wallpapers=Displays.config.wallpaper||({})
  fit()
 }
 function setOutput(name, values) {
  draft=draft.map(output=>output.name===name?Object.assign({},output,values):output)
  if(!draft.some(o=>o.name===primary&&o.enabled))primary=(draft.find(o=>o.enabled)||{}).name||""
  dirty=true
 }
 function place(output, x, y) {
  const size=extent(output)
  for(const other of draft) {
   if(other.name===output.name||!other.enabled)continue
   const edge=extent(other)
   if(Math.abs(x+size.width-other.x)<40)x=other.x-size.width
   else if(Math.abs(x-other.x-edge.width)<40)x=other.x+edge.width
   if(Math.abs(y+size.height-other.y)<40)y=other.y-size.height
   else if(Math.abs(y-other.y-edge.height)<40)y=other.y+edge.height
   if(Math.abs(y-other.y)<40)y=other.y
   if(Math.abs(x-other.x)<40)x=other.x
  }
  setOutput(output.name,{x:Math.round(x),y:Math.round(y)})
  fit()
 }
 function arrange(onlySelected) {
  let x=0
  draft=draft.map(output=>{
   const row=Object.assign({},output,{enabled:!onlySelected||output.name===selected,x:x,y:0})
   if(row.enabled)x+=extent(row).width
   return row
  })
  if(onlySelected)primary=selected
  dirty=true
  fit()
 }
 function mirror() {
  if(!active)return
  const base=modeOf(active)
  const choices=draft.map(output=>output.modes.find(mode=>mode.width===base.width&&mode.height===base.height))
  if(choices.some(mode=>!mode)){notice="These displays do not share the selected resolution. Choose a common resolution before mirroring.";return}
  draft=draft.map((output,index)=>Object.assign({},output,{mode:choices[index].id,enabled:true,x:0,y:0,scale:active.scale,transform:active.transform}))
  dirty=true;notice="Review this mirrored layout and apply to test it.";fit()
 }
 Component.onCompleted:Displays.detect()
 Connections {target:Displays;function onChanged(){panel.sync()}}
 SettingsLabel {text:"Display Arrangement";font.bold:true;font.pixelSize:17}
 SettingsLabel {text:"Drag displays to match their positions on your desk.";color:SettingsService.colors.muted}
 Rectangle {
  id:arrangement
  Layout.fillWidth:true
  Layout.preferredHeight:228
  radius:12
  color:SettingsService.colors.field
  border.color:SettingsService.colors.border
  clip:true
  Repeater {
   model:panel.draft
   delegate:Rectangle {
    id:tile
    required property var modelData
    required property int index
    property real dragX:0
    property real dragY:0
    width:Math.max(42,panel.extent(modelData).width*panel.mapScale-3)
    height:Math.max(35,panel.extent(modelData).height*panel.mapScale-3)
    x:16+(modelData.x-panel.bounds.left)*panel.mapScale+dragX
    y:16+(modelData.y-panel.bounds.top)*panel.mapScale+dragY
    z:pointer.pressed?2:modelData.name===panel.selected?1:0
    radius:9
    color:modelData.name===panel.selected?SettingsService.colors.accent:SettingsService.colors.surfaceHover
    border.color:SettingsService.colors.border
    opacity:modelData.enabled?1:.55
    Text {anchors.centerIn:parent;text:(tile.index+1)+(tile.modelData.name===panel.primary?" ★":"");color:tile.modelData.name===panel.selected?SettingsService.colors.accentText:SettingsService.colors.text;font.pixelSize:20;font.bold:true}
    MouseArea {
     id:pointer
     anchors.fill:parent
     enabled:!Displays.pending&&!Displays.busy
     property point origin
     onPressed:mouse=>{panel.selected=tile.modelData.name;origin=mapToItem(arrangement,mouse.x,mouse.y)}
     onPositionChanged:mouse=>{
      if(!pressed)return
      const pos=mapToItem(arrangement,mouse.x,mouse.y)
      tile.dragX=pos.x-origin.x
      tile.dragY=pos.y-origin.y
     }
     onReleased:{
      if(Math.abs(tile.dragX)+Math.abs(tile.dragY)>4)
       panel.place(tile.modelData,tile.modelData.x+tile.dragX/panel.mapScale,tile.modelData.y+tile.dragY/panel.mapScale)
      tile.dragX=0;tile.dragY=0
     }
     onCanceled:{tile.dragX=0;tile.dragY=0}
    }
   }
  }
  SettingsLabel {anchors.centerIn:parent;visible:panel.draft.length===0;text:Displays.busy?"Detecting displays…":"No configurable displays reported."}
 }
 Flow {
  Layout.fillWidth:true
  spacing:6
  SovelButton {text:"Detect Displays";enabled:!Displays.busy&&!Displays.pending;onClicked:{panel.dirty=false;Displays.detect()}}
  SovelButton {text:"Identify Displays";enabled:panel.draft.length>0&&!Displays.busy;onClicked:Displays.identify()}
  SovelButton {text:"Extend";enabled:panel.draft.length>1&&!Displays.pending;onClicked:panel.arrange(false)}
  SovelButton {text:"Mirror";enabled:panel.draft.length>1&&!!panel.active&&!Displays.pending;onClicked:panel.mirror()}
  SovelButton {text:"Only This Display";enabled:panel.draft.length>1&&!!panel.active&&!Displays.pending;onClicked:panel.arrange(true)}
 }
 SovelCombo {
  Layout.fillWidth:true
  model:panel.draft
  textRole:"description"
  currentIndex:panel.draft.findIndex(output=>output.name===panel.selected)
  onActivated:panel.selected=panel.draft[currentIndex].name
  Accessible.name:"Display to configure"
 }
 ColumnLayout {
  Layout.fillWidth:true
  visible:!!panel.active
  enabled:visible&&!Displays.busy&&!Displays.pending
  SettingsLabel {Layout.fillWidth:true;text:panel.active?panel.active.name+" · "+panel.active.connection+" · "+(panel.active.enabled?"Active":"Disabled"):"";color:SettingsService.colors.muted}
  RowLayout {
   Switch {
    text:"Enabled"
    checked:!!panel.active&&panel.active.enabled
    enabled:!!panel.active&&(!panel.active.enabled||panel.draft.filter(o=>o.enabled).length>1)
    onToggled:panel.setOutput(panel.selected,{enabled:checked})
   }
   SovelButton {text:panel.primary===panel.selected?"Primary":"Make Primary";enabled:!!panel.active&&panel.active.enabled&&panel.primary!==panel.selected;onClicked:{panel.primary=panel.selected;panel.dirty=true}}
  }
  SettingsLabel {text:"Resolution and refresh rate"}
  SovelCombo {
   Layout.fillWidth:true
   model:panel.active?panel.active.modes:[]
   textRole:"id"
   currentIndex:panel.active?panel.active.modes.findIndex(mode=>mode.id===panel.active.mode):-1
   enabled:count>0
   onActivated:{panel.setOutput(panel.selected,{mode:panel.active.modes[currentIndex].id});panel.fit()}
  }
  RowLayout {
   SettingsLabel {text:"Scale"}
   SovelCombo {
    model:["0.75","1","1.25","1.5","1.75","2","2.5","3","4"]
    currentIndex:panel.active?model.indexOf(String(panel.active.scale)):-1
    onActivated:{panel.setOutput(panel.selected,{scale:Number(currentText)});panel.fit()}
   }
   SettingsLabel {text:"Orientation"}
   SovelCombo {
    readonly property var values:["normal","90","180","270","flipped","flipped-90","flipped-180","flipped-270"]
    model:["Landscape","Portrait","Landscape inverted","Portrait inverted","Flipped","Flipped 90°","Flipped 180°","Flipped 270°"]
    currentIndex:panel.active?values.indexOf(panel.active.transform):0
    onActivated:{panel.setOutput(panel.selected,{transform:values[currentIndex]});panel.fit()}
   }
  }
  RowLayout {
   SovelButton {text:"Choose Wallpaper";onClicked:imagePicker.open()}
   SovelButton {text:"Use Desktop Wallpaper";enabled:!!panel.wallpapers[panel.selected];onClicked:{const next=Object.assign({},panel.wallpapers);delete next[panel.selected];panel.wallpapers=next;panel.dirty=true}}
  }
  SettingsLabel {Layout.fillWidth:true;text:panel.wallpapers[panel.selected]||"Uses the shared desktop wallpaper.";color:SettingsService.colors.muted;wrapMode:Text.WrapAnywhere}
 }
 RowLayout {
  SettingsLabel {text:"Top bar"}
  SovelCombo {model:["All displays","Primary only"];currentIndex:panel.barPlacement==="primary"?1:0;onActivated:{panel.barPlacement=currentIndex?"primary":"all";panel.dirty=true}}
  SettingsLabel {text:"Dock"}
  SovelCombo {model:["All displays","Primary only"];currentIndex:panel.dockPlacement==="primary"?1:0;onActivated:{panel.dockPlacement=currentIndex?"primary":"all";panel.dirty=true}}
 }
 RowLayout {
  SovelButton {text:"Apply";primary:true;enabled:panel.dirty&&!!panel.primary&&!Displays.busy&&!Displays.pending;onClicked:Displays.preview({outputs:panel.draft,primary:panel.primary,bar:panel.barPlacement,dock:panel.dockPlacement,wallpaper:panel.wallpapers})}
  SovelButton {text:"Keep Changes";visible:Displays.pending;enabled:!Displays.busy;onClicked:{panel.dirty=false;Displays.keep()}}
  SovelButton {text:"Revert";visible:Displays.pending;enabled:!Displays.busy;onClicked:{panel.dirty=false;Displays.revert()}}
 }
 SettingsLabel {Layout.fillWidth:true;visible:!!text;text:Displays.status;wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 SettingsLabel {Layout.fillWidth:true;visible:!!text;text:panel.notice;wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 Repeater {model:(Displays.config.outputs||[]).filter(saved=>!panel.draft.some(output=>output.name===saved.name))
  SettingsLabel {required property var modelData;Layout.fillWidth:true;text:(modelData.description||modelData.name)+" · Disconnected";color:SettingsService.colors.muted}
 }
 SettingsLabel {Layout.fillWidth:true;text:"Move a window between displays with Super+Shift+Arrow or drag its title bar.";wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 FileDialog {
  id:imagePicker
  title:"Choose wallpaper for this display"
  nameFilters:["Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif *.avif)"]
  onAccepted:{
   const next=Object.assign({},panel.wallpapers)
   next[panel.selected]=decodeURIComponent(selectedFile.toString().replace(/^file:\/\//,""))
   panel.wallpapers=next
   panel.dirty=true
  }
 }
}
