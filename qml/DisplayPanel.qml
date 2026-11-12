import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Display arranger. Selecting or dragging a display only edits a draft;
// nothing reaches the compositor until Apply, and an unconfirmed change
// reverts on its own after the countdown.
SettingsCard {
 id: panel
 property var draft: []
 property string selected: ""
 property string primary: ""
 property string barPlacement: "all"
 property string dockPlacement: "all"
 property var wallpapers: ({})
 property bool dirty: false
 property string notice: ""
 property int countdown: 0
 readonly property var active: draft.find(output=>output.name===selected) || null
 readonly property var enabledOutputs: draft.filter(output=>output.enabled)
 readonly property real snapDistance: 48
 property var bounds: ({left:0,top:0,right:1920,bottom:1080})
 readonly property real mapScale: Math.min((arrangement.width-56)/Math.max(1,bounds.right-bounds.left),
                                          (arrangement.height-56)/Math.max(1,bounds.bottom-bounds.top),0.3)
 readonly property real offsetX: (arrangement.width-(bounds.right-bounds.left)*mapScale)/2
 readonly property real offsetY: (arrangement.height-(bounds.bottom-bounds.top)*mapScale)/2

 function modeOf(output) {
  return output.modes.find(mode=>mode.id===output.mode) || output.modes[0] || {width:1920,height:1080,refresh:60}
 }
 function extent(output) {
  const mode=modeOf(output)
  const rotated=output.transform.endsWith("90")||output.transform.endsWith("270")
  return {width:Math.round((rotated?mode.height:mode.width)/output.scale),
          height:Math.round((rotated?mode.width:mode.height)/output.scale)}
 }
 function numberOf(output, index) { return output.number || index+1 }
 function resolutions(output) {
  const seen=[]
  for(const mode of output?output.modes:[]){const key=mode.width+" × "+mode.height;if(!seen.some(r=>r.label===key))seen.push({label:key,width:mode.width,height:mode.height})}
  return seen.sort((a,b)=>b.width*b.height-a.width*a.height)
 }
 function rates(output) {
  if(!output)return []
  const current=modeOf(output)
  return output.modes.filter(mode=>mode.width===current.width&&mode.height===current.height).sort((a,b)=>b.refresh-a.refresh)
 }
 function fit() {
  const shown=draft.length?draft:[]
  if(!shown.length)return
  bounds={left:Math.min(...shown.map(o=>o.x)),top:Math.min(...shown.map(o=>o.y)),
          right:Math.max(...shown.map(o=>o.x+extent(o).width)),
          bottom:Math.max(...shown.map(o=>o.y+extent(o).height))}
 }
 function sync() {
  if(dirty||Displays.pending||Displays.busy)return
  draft=Displays.outputs.map(o=>Object.assign({},o))
  if(!draft.some(o=>o.name===selected))selected=draft.length?(draft.find(o=>o.name===Displays.primary)||draft[0]).name:""
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
 function overlaps(a, b) {
  return a.x<b.x+b.width&&b.x<a.x+a.width&&a.y<b.y+b.height&&b.y<a.y+a.height
 }
 // Snap a dragged display to the nearest edge of its neighbours: beside them
 // (left or right) or above and below them, with edges aligned when close.
 function snapped(output, x, y) {
  const size=extent(output)
  let bestX=x,bestY=y,dx=snapDistance,dy=snapDistance
  for(const other of draft) {
   if(other.name===output.name||!other.enabled)continue
   const edge=extent(other)
   for(const candidate of [other.x-size.width,other.x+edge.width,other.x,other.x+edge.width-size.width])
    if(Math.abs(candidate-x)<dx){dx=Math.abs(candidate-x);bestX=candidate}
   for(const candidate of [other.y-size.height,other.y+edge.height,other.y,other.y+edge.height-size.height])
    if(Math.abs(candidate-y)<dy){dy=Math.abs(candidate-y);bestY=candidate}
  }
  return {x:Math.round(bestX),y:Math.round(bestY)}
 }
 // Displays may touch but never overlap: push the dropped one to the
 // closest free side of whatever it landed on.
 function separated(output, position) {
  const size=extent(output)
  let rect={x:position.x,y:position.y,width:size.width,height:size.height}
  for(let pass=0;pass<4;pass++){
   const other=draft.find(o=>o.name!==output.name&&o.enabled&&overlaps(rect,Object.assign({x:o.x,y:o.y},extent(o))))
   if(!other)break
   const edge=extent(other)
   const moves=[{x:other.x-size.width,y:rect.y},{x:other.x+edge.width,y:rect.y},{x:rect.x,y:other.y-size.height},{x:rect.x,y:other.y+edge.height}]
   moves.sort((a,b)=>Math.hypot(a.x-rect.x,a.y-rect.y)-Math.hypot(b.x-rect.x,b.y-rect.y))
   rect.x=moves[0].x;rect.y=moves[0].y
  }
  return {x:rect.x,y:rect.y}
 }
 function normalize() {
  const shown=draft.filter(o=>o.enabled)
  if(!shown.length)return
  const left=Math.min(...shown.map(o=>o.x)),top=Math.min(...shown.map(o=>o.y))
  if(left===0&&top===0)return
  draft=draft.map(o=>o.enabled?Object.assign({},o,{x:o.x-left,y:o.y-top}):o)
 }
 function place(output, x, y) {
  const position=separated(output,snapped(output,x,y))
  setOutput(output.name,position)
  normalize();fit()
 }
 function arrange(onlySelected) {
  let x=0
  const ordered=draft.slice().sort((a,b)=>a.x-b.x||a.y-b.y)
  const positions={}
  for(const output of ordered){
   const enabled=!onlySelected||output.name===selected
   positions[output.name]={enabled:enabled,x:enabled?x:output.x,y:0}
   if(enabled)x+=extent(Object.assign({},output,{enabled:true})).width
  }
  draft=draft.map(output=>Object.assign({},output,positions[output.name]))
  if(onlySelected)primary=selected
  notice=onlySelected?"Only the selected display stays on. Apply to test it.":"Displays placed side by side as one extended desktop. Apply to test it."
  dirty=true
  fit()
 }
 function mirror() {
  if(!active)return
  const base=modeOf(active)
  const choices=draft.map(output=>output.modes.find(mode=>mode.width===base.width&&mode.height===base.height))
  if(choices.some(mode=>!mode)){notice="These displays do not share the selected resolution. Choose a common resolution before mirroring.";return}
  draft=draft.map((output,index)=>Object.assign({},output,{mode:choices[index].id,enabled:true,x:0,y:0,scale:active.scale,transform:active.transform}))
  dirty=true;notice="Every display shows the same picture. Apply to test it.";fit()
 }
 function apply() {
  notice=""
  Displays.preview({outputs:draft,primary:primary,bar:barPlacement,dock:dockPlacement,wallpaper:wallpapers})
 }
 Component.onCompleted:Displays.detect()
 Connections {target:Displays;function onChanged(){
  if(Displays.pending&&panel.countdown===0){panel.countdown=15;tick.start()}
  if(!Displays.pending){panel.countdown=0;tick.stop()}
  panel.sync()
 }}
 Timer {id:tick;interval:1000;repeat:true;onTriggered:{if(panel.countdown>0)panel.countdown--;if(panel.countdown===0)stop()}}

 SettingsLabel {text:"Display Arrangement";font.bold:true;font.pixelSize:17}
 SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:"Drag displays to match where they sit on your desk. Displays snap to each other's edges. Nothing changes until you choose Apply.";color:SettingsService.colors.muted}
 Rectangle {
  id:arrangement
  Layout.fillWidth:true
  Layout.preferredHeight:300
  radius:14
  color:SettingsService.colors.field
  border.color:SettingsService.colors.border
  clip:true
  onWidthChanged:panel.fit()
  Repeater {
   model:panel.draft
   delegate:Rectangle {
    id:tile
    required property var modelData
    required property int index
    readonly property var size:panel.extent(modelData)
    readonly property bool chosen:modelData.name===panel.selected
    property real dragX:0
    property real dragY:0
    property var preview:null
    readonly property var shownAt:preview||{x:modelData.x,y:modelData.y}
    width:Math.max(56,size.width*panel.mapScale-4)
    height:Math.max(40,size.height*panel.mapScale-4)
    x:pointer.pressed?panel.offsetX+(modelData.x-panel.bounds.left)*panel.mapScale+dragX:panel.offsetX+(shownAt.x-panel.bounds.left)*panel.mapScale+2
    y:pointer.pressed?panel.offsetY+(modelData.y-panel.bounds.top)*panel.mapScale+dragY:panel.offsetY+(shownAt.y-panel.bounds.top)*panel.mapScale+2
    z:pointer.pressed?3:chosen?2:1
    radius:10
    color:chosen?Qt.tint(SettingsService.colors.surface,"#50"+String(SettingsService.colors.accent).slice(1)):SettingsService.colors.surface
    border.color:chosen?SettingsService.colors.accent:SettingsService.colors.border
    border.width:chosen?2:1
    opacity:modelData.enabled?1:.45
    Behavior on x {enabled:!pointer.pressed&&!System.reducedMotion;NumberAnimation {duration:140;easing.type:Easing.OutCubic}}
    Behavior on y {enabled:!pointer.pressed&&!System.reducedMotion;NumberAnimation {duration:140;easing.type:Easing.OutCubic}}
    // Live snap guide while dragging.
    Rectangle {
     visible:pointer.pressed&&!!tile.preview
     parent:arrangement;z:2;radius:10;color:"transparent";border.color:SettingsService.colors.accent;border.width:1
     opacity:.7
     x:tile.preview?panel.offsetX+(tile.preview.x-panel.bounds.left)*panel.mapScale+2:0
     y:tile.preview?panel.offsetY+(tile.preview.y-panel.bounds.top)*panel.mapScale+2:0
     width:tile.width;height:tile.height
    }
    Column {
     anchors.centerIn:parent;spacing:2;width:parent.width-12
     Text {width:parent.width;horizontalAlignment:Text.AlignHCenter;text:panel.numberOf(tile.modelData,tile.index)+(tile.modelData.name===panel.primary?"  ★":"");color:SettingsService.colors.text;font.pixelSize:Math.min(26,tile.height*.32);font.bold:true}
     Text {width:parent.width;horizontalAlignment:Text.AlignHCenter;visible:tile.height>70;elide:Text.ElideRight;text:(tile.modelData.model||tile.modelData.description||tile.modelData.name);color:SettingsService.colors.muted;font.pixelSize:11}
     Text {width:parent.width;horizontalAlignment:Text.AlignHCenter;visible:tile.height>90;elide:Text.ElideRight;text:tile.modelData.enabled?tile.size.width+" × "+tile.size.height:"Off";color:SettingsService.colors.muted;font.pixelSize:10}
    }
    MouseArea {
     id:pointer
     anchors.fill:parent
     enabled:!Displays.pending&&!Displays.busy
     cursorShape:pressed?Qt.ClosedHandCursor:Qt.OpenHandCursor
     property point origin
     onPressed:mouse=>{panel.selected=tile.modelData.name;origin=mapToItem(arrangement,mouse.x,mouse.y);tile.preview=null}
     onPositionChanged:mouse=>{
      if(!pressed)return
      const pos=mapToItem(arrangement,mouse.x,mouse.y)
      tile.dragX=pos.x-origin.x
      tile.dragY=pos.y-origin.y
      if(tile.modelData.enabled&&Math.abs(tile.dragX)+Math.abs(tile.dragY)>4)
       tile.preview=panel.separated(tile.modelData,panel.snapped(tile.modelData,tile.modelData.x+tile.dragX/panel.mapScale,tile.modelData.y+tile.dragY/panel.mapScale))
     }
     onReleased:{
      if(tile.modelData.enabled&&Math.abs(tile.dragX)+Math.abs(tile.dragY)>4)
       panel.place(tile.modelData,tile.modelData.x+tile.dragX/panel.mapScale,tile.modelData.y+tile.dragY/panel.mapScale)
      tile.dragX=0;tile.dragY=0;tile.preview=null
     }
     onCanceled:{tile.dragX=0;tile.dragY=0;tile.preview=null}
    }
   }
  }
  SettingsLabel {anchors.centerIn:parent;visible:panel.draft.length===0;text:Displays.busy?"Detecting displays…":"No configurable displays reported."}
 }
 Flow {
  Layout.fillWidth:true
  spacing:6
  SovelButton {text:"Identify";enabled:panel.draft.length>0&&!Displays.busy;onClicked:Displays.identify()}
  SovelButton {text:"Detect";enabled:!Displays.busy&&!Displays.pending;onClicked:{panel.dirty=false;panel.notice="";Displays.detect()}}
  SovelButton {text:"Extend";enabled:panel.draft.length>1&&!Displays.pending;onClicked:panel.arrange(false)}
  SovelButton {text:"Duplicate";enabled:panel.draft.length>1&&!!panel.active&&!Displays.pending;onClicked:panel.mirror()}
  SovelButton {text:"Only This Display";enabled:panel.draft.length>1&&!!panel.active&&!Displays.pending;onClicked:panel.arrange(true)}
 }
 // Display tabs: choosing one only selects it for editing.
 Flow {
  Layout.fillWidth:true;spacing:6
  Repeater {model:panel.draft
   SovelButton {required property var modelData;required property int index
    text:panel.numberOf(modelData,index)+"  "+(modelData.model||modelData.name)
    primary:modelData.name===panel.selected
    onClicked:panel.selected=modelData.name}
  }
 }
 GridLayout {
  id:form
  Layout.fillWidth:true
  visible:!!panel.active
  enabled:visible&&!Displays.busy&&!Displays.pending
  columns:width>560?2:1;columnSpacing:14;rowSpacing:10
  SettingsLabel {Layout.columnSpan:form.columns;Layout.fillWidth:true;wrapMode:Text.WordWrap
   text:panel.active?(panel.active.description||panel.active.name)+" · "+panel.active.name+(panel.active.physicalWidth?" · "+Math.round(Math.hypot(panel.active.physicalWidth,panel.active.physicalHeight)/25.4)+"″":""):""
   color:SettingsService.colors.muted}
  Flow {Layout.columnSpan:form.columns;Layout.fillWidth:true;spacing:12
   Switch {
    text:"Use this display"
    palette.text:SettingsService.colors.text;palette.windowText:SettingsService.colors.text
    checked:!!panel.active&&panel.active.enabled
    enabled:!!panel.active&&(!panel.active.enabled||panel.enabledOutputs.length>1)
    onToggled:{panel.setOutput(panel.selected,{enabled:checked});if(checked)panel.place(panel.active,panel.bounds.right,0);panel.fit()}
   }
   SovelButton {text:panel.primary===panel.selected?"Primary display":"Make Primary";enabled:!!panel.active&&panel.active.enabled&&panel.primary!==panel.selected;onClicked:{panel.primary=panel.selected;panel.dirty=true}}
  }
  SettingsLabel {text:"Resolution"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var choices:panel.resolutions(panel.active)
   model:choices;textRole:"label"
   currentIndex:panel.active?choices.findIndex(r=>r.width===panel.modeOf(panel.active).width&&r.height===panel.modeOf(panel.active).height):-1
   enabled:count>0
   onActivated:{
    const choice=choices[currentIndex]
    const candidates=panel.active.modes.filter(m=>m.width===choice.width&&m.height===choice.height).sort((a,b)=>b.refresh-a.refresh)
    const keep=candidates.find(m=>Math.abs(m.refresh-panel.modeOf(panel.active).refresh)<0.5)||candidates[0]
    panel.setOutput(panel.selected,{mode:keep.id});panel.place(panel.active,panel.active.x,panel.active.y)
   }
  }
  SettingsLabel {text:"Refresh rate"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var choices:panel.rates(panel.active).map(m=>({id:m.id,label:(Math.round(m.refresh*100)/100)+" Hz"}))
   model:choices;textRole:"label"
   currentIndex:panel.active?choices.findIndex(c=>c.id===panel.active.mode):-1
   enabled:count>1
   onActivated:panel.setOutput(panel.selected,{mode:choices[currentIndex].id})
  }
  SettingsLabel {text:"Scale"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var values:[0.75,1,1.25,1.5,1.75,2,2.5,3]
   model:values.map(v=>Math.round(v*100)+"%")
   currentIndex:panel.active?values.findIndex(v=>Math.abs(v-panel.active.scale)<0.01):-1
   displayText:currentIndex<0&&panel.active?Math.round(panel.active.scale*100)+"%":currentText
   onActivated:{panel.setOutput(panel.selected,{scale:values[currentIndex]});panel.place(panel.active,panel.active.x,panel.active.y)}
  }
  SettingsLabel {text:"Orientation"}
  SovelCombo {
   Layout.fillWidth:true;Layout.preferredWidth:1
   readonly property var values:["normal","90","180","270","flipped","flipped-90","flipped-180","flipped-270"]
   model:["Landscape","Portrait","Landscape (flipped)","Portrait (flipped)","Mirrored","Mirrored portrait","Mirrored, upside down","Mirrored portrait (flipped)"]
   currentIndex:panel.active?values.indexOf(panel.active.transform):0
   onActivated:{panel.setOutput(panel.selected,{transform:values[currentIndex]});panel.place(panel.active,panel.active.x,panel.active.y)}
  }
  SettingsLabel {text:"Variable refresh (VRR)"}
  RowLayout {Layout.fillWidth:true;Layout.preferredWidth:1;spacing:8
   Switch {
    id:vrr
    enabled:!!panel.active&&panel.active.adaptiveSync!==undefined
    checked:!!panel.active&&!!panel.active.adaptiveSync
    onToggled:panel.setOutput(panel.selected,{adaptiveSync:checked})
    Accessible.name:"Variable refresh"
   }
   SettingsLabel {Layout.fillWidth:true;visible:!vrr.enabled;wrapMode:Text.WordWrap;color:SettingsService.colors.muted;text:"Not reported by this display."}
  }
  SettingsLabel {text:"HDR"}
  SettingsLabel {Layout.fillWidth:true;Layout.preferredWidth:1;wrapMode:Text.WordWrap;color:SettingsService.colors.muted;text:"Not available: the compositor does not support HDR output yet."}
  SettingsLabel {text:"Wallpaper"}
  RowLayout {
   SovelButton {text:"Choose…";onClicked:imagePicker.open()}
   SovelButton {text:"Use desktop wallpaper";enabled:!!panel.wallpapers[panel.selected];onClicked:{const next=Object.assign({},panel.wallpapers);delete next[panel.selected];panel.wallpapers=next;panel.dirty=true}}
  }
  Item {width:1;height:1;visible:form.columns===2}
  SettingsLabel {Layout.fillWidth:true;Layout.preferredWidth:1;text:panel.wallpapers[panel.selected]||(System.privateMode?"Uses the private workspace wallpaper.":"Uses the desktop wallpaper.");color:SettingsService.colors.muted;wrapMode:Text.WrapAnywhere}
 }
 Flow {
  Layout.fillWidth:true;spacing:10
  SettingsLabel {text:"Menu bar";height:38;verticalAlignment:Text.AlignVCenter}
  SovelCombo {model:["All displays","Primary only"];currentIndex:panel.barPlacement==="primary"?1:0;onActivated:{panel.barPlacement=currentIndex?"primary":"all";panel.dirty=true}}
  SettingsLabel {text:"Dock";height:38;verticalAlignment:Text.AlignVCenter}
  SovelCombo {model:["All displays","Primary only"];currentIndex:panel.dockPlacement==="primary"?1:0;onActivated:{panel.dockPlacement=currentIndex?"primary":"all";panel.dirty=true}}
 }
 Rectangle {
  Layout.fillWidth:true;visible:Displays.pending;radius:12;implicitHeight:pendingRow.implicitHeight+20
  color:Qt.tint(SettingsService.colors.surface,"#30"+String(SettingsService.colors.accent).slice(1));border.color:SettingsService.colors.accent
  RowLayout {id:pendingRow;anchors.fill:parent;anchors.margins:10;spacing:10
   SettingsLabel {Layout.fillWidth:true;wrapMode:Text.WordWrap;text:"Keep this arrangement? It reverts in "+panel.countdown+" s if you do nothing."}
   SovelButton {text:"Keep Changes";primary:true;enabled:!Displays.busy;onClicked:{panel.dirty=false;Displays.keep()}}
   SovelButton {text:"Revert";enabled:!Displays.busy;onClicked:{panel.dirty=false;Displays.revert()}}
  }
 }
 RowLayout {
  visible:!Displays.pending
  SovelButton {text:"Apply";primary:true;enabled:panel.dirty&&!!panel.primary&&!Displays.busy;onClicked:panel.apply()}
  SovelButton {text:"Discard";enabled:panel.dirty&&!Displays.busy;onClicked:{panel.dirty=false;panel.notice="";panel.sync()}}
 }
 SettingsLabel {Layout.fillWidth:true;visible:!!text;text:Displays.status;wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 SettingsLabel {Layout.fillWidth:true;visible:!!text;text:panel.notice;wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
 Repeater {model:(Displays.config.outputs||[]).filter(saved=>!panel.draft.some(output=>output.name===saved.name))
  SettingsLabel {required property var modelData;Layout.fillWidth:true;text:(modelData.description||modelData.name)+" · Disconnected · its saved position returns when it is plugged in";color:SettingsService.colors.muted;wrapMode:Text.WordWrap}
 }
 SettingsLabel {Layout.fillWidth:true;text:"Move a window between displays with Super+Shift+Arrow or drag its title bar. "+(System.privateMode?"Private Mode keeps its own arrangement.":"Normal Mode keeps its own arrangement.");wrapMode:Text.WordWrap;color:SettingsService.colors.muted}
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
