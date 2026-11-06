import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
 id:root
 property string kind:"clock"
 property string widgetId:""
 property string widgetSize:"medium"
 property int widgetOpacity:75
 property int refreshSeconds:2
 property bool editing:false
 property real positionX:0
 property real positionY:0
 property int savedWidth:0
 property int savedHeight:0
 property rect startBounds
 property rect previewBounds
 property bool resizing:false
 property bool moving:false
 readonly property bool dragging:moving||resizing
 readonly property bool desktopCanvas:parent&&parent.width>=200&&parent.height>=208
 readonly property real limitX:desktopCanvas?parent.width-12:4096
 readonly property real limitY:desktopCanvas?parent.height-16:2160
 readonly property bool compact:width<270
 readonly property bool expanded:width>=370&&height>=280
 property var localData:Workspace.data[widgetId]||({})
 property var metrics:Object.keys(Workspace.telemetry).length?Workspace.telemetry:System.metrics
 property date now:new Date()
 readonly property var definition:Workspace.registry.find(w=>w.type===kind)||({name:kind})
 readonly property bool privateHidden:SettingsService.preferences["widgets.private"]===true
 readonly property string textColor:SettingsService.colors.text
 readonly property string mutedColor:SettingsService.colors.muted
 signal placed(real x,real y,int width,int height)
 width:dragging?previewBounds.width:Math.min(limitX-12,savedWidth||(widgetSize==="small"?204:widgetSize==="large"?424:300))
 height:dragging?previewBounds.height:Math.min(limitY-42,savedHeight||(widgetSize==="small"?204:widgetSize==="large"?340:236))
 x:dragging?previewBounds.x:clamp(positionX*(limitX+12-width),12,limitX-width)
 y:dragging?previewBounds.y:clamp(positionY*(limitY+16-height),42,limitY-height)
 z:dragging?1:0
 scale:mover.active&&!System.reducedMotion&&SettingsService.preferences["widgets.animations"]?1.025:1
 Behavior on scale {enabled:!System.reducedMotion&&SettingsService.preferences["widgets.animations"];NumberAnimation {duration:140;easing.type:Easing.OutCubic}}
 Behavior on width {enabled:!root.dragging&&!System.reducedMotion&&SettingsService.preferences["widgets.animations"];NumberAnimation {duration:180;easing.type:Easing.OutCubic}}
 Behavior on height {enabled:!root.dragging&&!System.reducedMotion&&SettingsService.preferences["widgets.animations"];NumberAnimation {duration:180;easing.type:Easing.OutCubic}}
 function clamp(value,minimum,maximum){return Math.max(minimum,Math.min(maximum,value))}
 function snap(value){return SettingsService.preferences["widgets.snapToGrid"]?Math.round(value/16)*16:value}
 function beginPlacement(){startBounds=Qt.rect(x,y,width,height);previewBounds=startBounds}
 function commitPlacement(resized){
  const bounds=previewBounds
  if(bounds.x===startBounds.x&&bounds.y===startBounds.y&&bounds.width===startBounds.width&&bounds.height===startBounds.height)return
  placed(bounds.x/Math.max(1,limitX+12-bounds.width),bounds.y/Math.max(1,limitY+16-bounds.height),
         resized?Math.round(bounds.width):savedWidth,resized?Math.round(bounds.height):savedHeight)
 }
 function resizePlacement(dx,dy,horizontal,vertical){
  const start=startBounds
  let left=start.x,top=start.y,right=left+start.width,bottom=top+start.height
  if(horizontal<0)left=clamp(snap(left+dx),Math.max(12,right-4096),right-176)
  if(horizontal>0)right=clamp(snap(right+dx),left+176,Math.min(limitX,left+4096))
  if(vertical<0)top=clamp(snap(top+dy),Math.max(42,bottom-2160),bottom-150)
  if(vertical>0)bottom=clamp(snap(bottom+dy),top+150,Math.min(limitY,top+2160))
  previewBounds=Qt.rect(left,top,right-left,bottom-top)
 }
 function save(values){Workspace.setWidgetData(widgetId,Object.assign({},localData,values))}
 function metricRows(){
  const battery=(metrics.batteries||[])[0]||{};
  const values={cpu:[{label:"CPU",value:metrics.cpu>=0?Number(metrics.cpu).toFixed(0)+"%":"Sampling…",ratio:Number(metrics.cpu||0)/100}],gpu:[{label:metrics.gpuName||"GPU",value:metrics.gpuAvailable?Number(metrics.gpu).toFixed(0)+"%":"Not exposed by this driver",ratio:Number(metrics.gpu||0)/100}],ram:[{label:"Memory",value:Number(metrics.ramUsed||0).toFixed(1)+" / "+Number(metrics.ramTotal||0).toFixed(1)+" GB",ratio:metrics.ramTotal?metrics.ramUsed/metrics.ramTotal:0}],storage:[{label:"Available storage",value:Number(metrics.diskFree||0).toFixed(1)+" GB",ratio:metrics.diskTotal?1-metrics.diskFree/metrics.diskTotal:0}],network:[{label:"Download",value:(Number(metrics.rxPerSecond||0)/1024).toFixed(1)+" KB/s"},{label:"Upload",value:(Number(metrics.txPerSecond||0)/1024).toFixed(1)+" KB/s"}],battery:[{label:battery.name||"Battery",value:battery.percent>=0?battery.percent+"% · "+battery.status:"No battery reported",ratio:battery.percent>=0?battery.percent/100:0}]};
  return kind==="system"?[values.cpu[0],values.ram[0],values.storage[0],values.gpu[0]]:values[kind]||[];
 }
 ShellGlass {anchors.fill:parent;tint:Math.max(.1,Math.min(1,root.widgetOpacity/100*Number(SettingsService.preferences["widgets.opacity"]||75)/75))}
 DragHandler {
  id:mover;enabled:root.editing&&!root.resizing;target:null
  onActiveChanged:{
   if(active){root.beginPlacement();root.moving=true}
   else if(root.moving){if(root.editing)root.commitPlacement(false);root.moving=false}
  }
  onActiveTranslationChanged:{
   if(!active)return
   const start=root.startBounds
   root.previewBounds=Qt.rect(root.clamp(root.snap(start.x+activeTranslation.x),12,root.limitX-start.width),
                             root.clamp(root.snap(start.y+activeTranslation.y),42,root.limitY-start.height),start.width,start.height)
  }
 }
 Rectangle {anchors.fill:parent;radius:24;visible:root.editing;color:"transparent";border.color:SettingsService.colors.accent;border.width:2}
 Timer {interval:root.kind==="analog"?1000:Math.max(1000,root.refreshSeconds*1000);repeat:true;running:root.visible&&["clock","analog","world","date","calendar","timer","reminders","events"].includes(root.kind);onTriggered:root.now=new Date()}
 ColumnLayout {anchors.fill:parent;anchors.margins:17;spacing:10
  RowLayout {Layout.fillWidth:true
   Text {text:root.definition.name;color:root.mutedColor;font.pixelSize:11;font.weight:Font.DemiBold;Layout.fillWidth:true;elide:Text.ElideRight}
   ToolButton {visible:root.editing;text:"⚙";implicitHeight:24;implicitWidth:24;onClicked:widgetMenu.open();Accessible.name:"Configure widget"}
  }
  Loader {id:body;Layout.fillWidth:true;Layout.fillHeight:true
   sourceComponent:["cpu","gpu","ram","storage","network","battery","system"].includes(root.kind)?telemetryPanel:["clock","date","world"].includes(root.kind)?clockPanel:root.kind==="analog"?analogPanel:root.kind==="calendar"?calendarPanel:root.kind==="notes"?notesPanel:["todo","reminders","events"].includes(root.kind)?listPanel:root.kind==="timer"?timerPanel:root.kind==="calculator"?calculatorPanel:root.kind==="weather"?weatherPanel:root.kind==="media"?mediaPanel:root.kind==="downloads"?downloadsPanel:root.kind==="updates"?updatePanel:root.kind==="actions"?actionsPanel:jaredPanel
  }
 }
 component Copy: Text {color:root.textColor;wrapMode:Text.Wrap;textFormat:Text.PlainText;font.pixelSize:13;Layout.fillWidth:true}
 Component {id:telemetryPanel;ScrollView {id:telemetryScroll;clip:true;contentWidth:availableWidth
  GridLayout {width:telemetryScroll.availableWidth;columns:root.kind==="system"&&!root.compact?2:1;columnSpacing:14;rowSpacing:14;Repeater {model:root.metricRows()
   ColumnLayout {required property var modelData;Layout.fillWidth:true;spacing:5
    Copy {text:modelData.label;color:root.mutedColor;font.pixelSize:10}
    Copy {text:modelData.value;font.pixelSize:root.kind==="system"?16:root.compact?19:24}
    ProgressBar {id:metricBar;visible:modelData.ratio!==undefined;Layout.fillWidth:true;Layout.preferredHeight:5;from:0;to:1;value:Math.max(0,Math.min(1,Number(modelData.ratio||0)));background:Rectangle {radius:3;color:"#25888888"}contentItem:Item {Rectangle {width:parent.width*metricBar.visualPosition;height:parent.height;radius:3;color:SettingsService.colors.accent}}}
   }
  }}
 }}
 Component {id:clockPanel;ColumnLayout {spacing:9
  Copy {text:{const instant=root.now;return root.kind==="world"?Workspace.timeInZone(root.localData.zone||"UTC"):root.kind==="date"?Qt.formatDate(instant,"dddd, MMMM d"):Qt.formatTime(instant,"h:mm AP")}font.pixelSize:root.compact?25:32}
  Copy {text:root.kind==="world"?(root.localData.zone||"UTC"):Qt.formatDate(root.now,"dddd, MMMM d, yyyy");color:root.mutedColor;font.pixelSize:12}
  TextField {visible:root.kind==="world";Layout.fillWidth:true;placeholderText:"IANA zone, e.g. Europe/London";text:root.localData.zone||"UTC";onAccepted:root.save({zone:text})}
  Item {Layout.fillHeight:true}
 }}
 Component {id:analogPanel;Item {Canvas {id:face;anchors.centerIn:parent;width:Math.min(parent.width,parent.height);height:width;property date instant:root.now;onInstantChanged:requestPaint();onPaint:{const c=getContext("2d"),r=width/2;c.reset();c.translate(r,r);c.strokeStyle=root.textColor;c.lineWidth=2;c.beginPath();c.arc(0,0,r-3,0,2*Math.PI);c.stroke();for(let i=0;i<12;i++){c.save();c.rotate(i*Math.PI/6);c.beginPath();c.moveTo(0,-r+9);c.lineTo(0,-r+16);c.stroke();c.restore()}const h=instant.getHours()%12+instant.getMinutes()/60,m=instant.getMinutes()+instant.getSeconds()/60;for(const hand of [{a:h*Math.PI/6,l:.45,w:4},{a:m*Math.PI/30,l:.7,w:2}]){c.save();c.rotate(hand.a);c.lineWidth=hand.w;c.beginPath();c.moveTo(0,6);c.lineTo(0,-r*hand.l);c.stroke();c.restore()}}}}}
 Component {id:calendarPanel;Item {
  Copy {anchors.centerIn:parent;width:parent.width;text:Qt.formatDate(root.now,"MMMM yyyy\ndddd d");horizontalAlignment:Text.AlignHCenter;font.pixelSize:24;visible:!root.expanded}
  CalendarPanel {anchors.fill:parent;visible:root.expanded}
  TapHandler {enabled:!root.expanded;onTapped:calendarPopup.open()}
 }}
 Component {id:notesPanel;ScrollView {clip:true;TextArea {text:root.privateHidden?"Hidden by widget privacy":root.localData.text||"";readOnly:root.privateHidden;placeholderText:"Write a local note…";wrapMode:TextEdit.Wrap;color:root.textColor;onTextChanged:if(activeFocus&&!root.privateHidden&&text.length<=6000)noteSave.restart();onActiveFocusChanged:if(!activeFocus&&noteSave.running){noteSave.stop();root.save({text:text})} Timer {id:noteSave;interval:500;onTriggered:root.save({text:parent.text})}}}}
 Component {id:listPanel;ColumnLayout {spacing:6
  Copy {visible:root.privateHidden;text:"Content hidden by widget privacy";color:root.mutedColor}
  ListView {id:tasks;visible:!root.privateHidden;Layout.fillWidth:true;Layout.fillHeight:true;clip:true;model:root.localData.items||[];spacing:4
   delegate:RowLayout {required property var modelData;required property int index;width:tasks.width
    CheckBox {checked:!!modelData.done;onToggled:{let items=(root.localData.items||[]).slice();items[index]=Object.assign({},modelData,{done:checked});root.save({items:items})}}
    Copy {text:modelData.text+(modelData.due?"\n"+Qt.formatDateTime(new Date(modelData.due),"ddd h:mm AP"):"");font.strikeout:!!modelData.done;font.pixelSize:12}
    ToolButton {text:"×";implicitWidth:24;onClicked:{let items=(root.localData.items||[]).slice();items.splice(index,1);root.save({items:items})} Accessible.name:"Delete local item"}
   }
  }
  RowLayout {visible:!root.privateHidden;Layout.fillWidth:true
   TextField {id:newItem;Layout.fillWidth:true;maximumLength:240;placeholderText:root.kind==="todo"?"Add a task":"Add an item";onAccepted:addItem.clicked()}
   SovelButton {id:addItem;text:"+";implicitWidth:32;onClicked:if(newItem.text.trim()){let items=(root.localData.items||[]).slice();if(items.length>=30)return;items.push({text:newItem.text.trim(),done:false,due:root.kind==="todo"?"":new Date(Date.now()+Number(root.localData.minutes||30)*60000).toISOString()});root.save({items:items});newItem.clear()}}
  }
  RowLayout {visible:root.kind!=="todo"&&!root.privateHidden
   Copy {text:"Remind in minutes";font.pixelSize:10}
   SpinBox {from:1;to:10080;value:root.localData.minutes||30;onValueModified:root.save({minutes:value})}
  }
 }}
 Component {id:timerPanel;ColumnLayout {id:timerBody;spacing:8
  readonly property int remaining:root.localData.deadline?Math.max(0,Math.ceil((Number(root.localData.deadline)-root.now.getTime())/1000)):Number(root.localData.seconds===undefined?300:root.localData.seconds)
  Copy {text:Math.floor(timerBody.remaining/60)+":"+String(timerBody.remaining%60).padStart(2,"0");font.pixelSize:34}
  Flow {Layout.fillWidth:true;spacing:6;SpinBox {implicitWidth:76;from:1;to:1440;value:Math.ceil(Number(root.localData.seconds||300)/60);enabled:!root.localData.deadline;onValueModified:root.save({seconds:value*60})}
   SovelButton {text:root.localData.deadline?"Pause":"Start";onClicked:if(root.localData.deadline)root.save({seconds:timerBody.remaining,deadline:0});else root.save({deadline:Date.now()+Number(root.localData.seconds||300)*1000,notified:false})}
   SovelButton {text:"Reset";onClicked:root.save({seconds:300,deadline:0,notified:false})}
  }
 }}
 Component {id:calculatorPanel;ColumnLayout {TextField {id:expression;Layout.fillWidth:true;maximumLength:100;placeholderText:"12 * 4";onAccepted:answer.text=Workspace.calculate(text)} Copy {id:answer;text:"Two-number arithmetic";font.pixelSize:22} SovelButton {text:"Calculate";onClicked:answer.text=Workspace.calculate(expression.text)} Item {Layout.fillHeight:true}}}
 Component {id:weatherPanel;ColumnLayout {spacing:8
  Copy {text:!SettingsService.preferences["weather.enabled"]?"Weather access is off":Workspace.weather.available?Math.round(Workspace.weather.temperature)+" °C · "+Workspace.weather.city:"Choose a city in Settings";font.pixelSize:23}
  Repeater {model:root.expanded&&SettingsService.preferences["weather.enabled"]?Workspace.weather.forecast||[]:[];Copy {required property var modelData;text:modelData.date+"  "+Math.round(modelData.high)+"° / "+Math.round(modelData.low)+"°";font.pixelSize:12}}
  Copy {text:Workspace.weather.status||"Manual city only · no automatic location";font.pixelSize:10;color:root.mutedColor}
  SovelButton {text:"Weather settings";onClicked:System.launch("settings")}
 }}
 Component {id:mediaPanel;ColumnLayout {Copy {text:Workspace.media.available?Workspace.media.title||"Media playback":"No media player is available";font.pixelSize:20}Copy {text:Workspace.media.artist||"";color:root.mutedColor}RowLayout {SovelButton {text:"◀";enabled:!!Workspace.media.canPrevious;onClicked:Workspace.mediaAction("Previous")}SovelButton {text:Workspace.media.playing?"Pause":"Play";enabled:!!Workspace.media.canControl;onClicked:Workspace.mediaAction("PlayPause")}SovelButton {text:"▶";enabled:!!Workspace.media.canNext;onClicked:Workspace.mediaAction("Next")}}Item {Layout.fillHeight:true}}}
 Component {id:downloadsPanel;ColumnLayout {Copy {text:root.privateHidden?"Download names hidden":"Recent downloaded files";color:root.mutedColor;font.pixelSize:11}Repeater {model:root.privateHidden?[]:metrics.downloads||[];Copy {required property string modelData;text:modelData;maximumLineCount:1;elide:Text.ElideMiddle}}SovelButton {text:"Open Files";onClicked:System.launch("files")}Item {Layout.fillHeight:true}}}
 Component {id:updatePanel;ColumnLayout {Copy {text:Updates.busy?Updates.status:Updates.packages.length?Updates.packages.length+" updates available":Updates.checked?"Your system is up to date":"Check for system updates";font.pixelSize:19}SovelButton {text:"Software Update";onClicked:System.launch("updates")}Item {Layout.fillHeight:true}}}
 Component {id:actionsPanel;Flow {width:parent.width;spacing:8;SovelButton {text:"Files";onClicked:System.launch("files")}SovelButton {text:"Settings";onClicked:System.launch("settings")}SovelButton {text:"Lock";onClicked:System.launch("lock")}SovelButton {text:"Library";onClicked:System.launch("library")}}}
 Component {id:jaredPanel;JaredWorkspaceWidget {kind:root.kind;widgetId:root.widgetId;privateHidden:root.privateHidden;metrics:root.metrics}}
 Repeater {
  model:root.editing&&root.desktopCanvas?[[-1,-1],[0,-1],[1,-1],[-1,0],[1,0],[-1,1],[0,1],[1,1]]:[]
  MouseArea {
   required property var modelData
   readonly property int horizontal:modelData[0]
   readonly property int vertical:modelData[1]
   property point origin
   width:horizontal===0?root.width-24:12
   height:vertical===0?root.height-24:12
   x:horizontal<0?0:horizontal>0?root.width-width:12
   y:vertical<0?0:vertical>0?root.height-height:12
   z:2
   cursorShape:horizontal===0?Qt.SizeVerCursor:vertical===0?Qt.SizeHorCursor:horizontal===vertical?Qt.SizeFDiagCursor:Qt.SizeBDiagCursor
   preventStealing:true
   onPressed:mouse=>{
    origin=mapToItem(root.parent,mouse.x,mouse.y)
    root.beginPlacement()
    root.resizing=true
   }
   onPositionChanged:mouse=>{
    if(!pressed)return
    const point=mapToItem(root.parent,mouse.x,mouse.y)
    root.resizePlacement(point.x-origin.x,point.y-origin.y,horizontal,vertical)
   }
   onReleased:{root.commitPlacement(true);root.resizing=false}
   onCanceled:root.resizing=false
  }
 }
 ShellMenu {id:widgetMenu;ShellMenuItem {text:"Use preset size";visible:root.savedWidth>0;onTriggered:root.placed(root.positionX,root.positionY,0,0)}ShellMenuItem {text:"Small";onTriggered:Workspace.configureWidget(root.widgetId,"small",root.widgetOpacity,root.refreshSeconds)}ShellMenuItem {text:"Medium";onTriggered:Workspace.configureWidget(root.widgetId,"medium",root.widgetOpacity,root.refreshSeconds)}ShellMenuItem {text:"Large";onTriggered:Workspace.configureWidget(root.widgetId,"large",root.widgetOpacity,root.refreshSeconds)}ShellMenuItem {text:"Remove widget";onTriggered:Workspace.removeWidget(root.widgetId)}}
 ShellPopup {id:calendarPopup;width:350;height:calendarContent.implicitHeight+32;anchors.centerIn:Overlay.overlay;contentItem:CalendarPanel {id:calendarContent}}
}
