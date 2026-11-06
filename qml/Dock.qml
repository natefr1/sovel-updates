import QtQuick
import QtQuick.Controls
Item {
 id:root
 property var windows:Windows.entries
 property string edge:System.shellPreferences.activationEdge || "bottom"
 readonly property bool vertical:edge==="left"||edge==="right"
 property bool dragging:false
 property string draggedId:""
 property real dragX:0
 property real dragY:0
 property string dragIcon:""
 property int pinCount:0
 property var catalog:[]
 property var pending:({})
 function key(value){return (value||"").replace(/\.desktop$/i,"").toLowerCase()}
 function matches(app,window){return (app.appIds||[]).some(id=>key(id)===key(window.appId))}
 function rebuild(){
  if(dragging||!dockModel)return;
  catalog=System.applications();let rows=[];let seen={};
  for(const app of System.pinnedApps){rows.push({app:app,pinned:true,windowId:0});seen[app.id]=true}
  pinCount=rows.length;
  for(const window of root.windows){
   if(!window.appId)continue;
   let app=catalog.find(a=>matches(a,window))||{id:"running:"+window.appId,name:window.name,icon:"image://apps/"+encodeURIComponent(window.appId),appIds:[window.appId]};
   if(app.id==="sovel:library"||window.appId==="sovel-library")continue;
   if(!seen[app.id]){rows.push({app:app,pinned:false,windowId:window.id});seen[app.id]=true}
  }
  rows.push({app:{id:"sovel:library",name:"App Library",icon:"qrc:/assets/icons/library.svg",appIds:["sovel-library"]},pinned:false,windowId:0});
  for(let i=dockModel.count-1;i>=0;i--)if(!rows.some(r=>r.app.id===dockModel.get(i).appKey))dockModel.remove(i);
  for(let i=0;i<rows.length;i++){
   const row=rows[i];let from=-1;for(let j=i;j<dockModel.count;j++)if(dockModel.get(j).appKey===row.app.id){from=j;break}
   const values={appKey:row.app.id,appName:row.app.name,icon:row.app.icon,pinned:row.pinned,windowId:row.windowId,library:row.app.id==="sovel:library"};
   if(from<0)dockModel.insert(i,values);else{if(from!==i)dockModel.move(from,i,1);dockModel.set(i,values)}
  }
 }
 function appFor(id){return catalog.find(a=>a.id===id)||{id:id,appIds:id==="sovel:library"?["sovel-library"]:id.startsWith("running:")?[id.slice(8)]:[],name:"App"}}
 function activate(id,windowId){
  const app=appFor(id);
  if(!Windows.activateApp(app.appIds||[],true)){
   if(id.startsWith("running:")||(pending[id]||0)>Date.now())return;
   pending[id]=Date.now()+1500;
   if(id==="sovel:library")System.launch("library");else System.launchApplication(id);
  }
 }
 function reorderAt(scenePosition){
  if(!dragging)return;
  const point=root.mapFromItem(null,scenePosition.x,scenePosition.y);dragX=point.x;dragY=point.y;
  let from=-1;for(let i=0;i<pinCount;i++)if(dockModel.get(i).appKey===draggedId)from=i;
  if(from<0)return;
  for(let i=0;i<pinCount;i++){
   const item=items.itemAt(i);if(!item)continue;const p=item.mapToItem(root,item.width/2,item.height/2);
   if(Math.abs((vertical?p.y:p.x)-(vertical?dragY:dragX))<(vertical?item.height:item.width)/2+8&&i!==from){dockModel.move(from,i,1);break}
  }
 }
 function saveOrder(){let ids=[];for(let i=0;i<pinCount;i++)ids.push(dockModel.get(i).appKey);System.setDockOrder(ids);dragging=false;Qt.callLater(rebuild)}
 Component.onCompleted:rebuild()
 Connections {target:System;function onPreferencesChanged(){root.rebuild()}}
 onWindowsChanged:rebuild()
 ListModel {id:dockModel}
 property real panelLength:Math.min((vertical?root.height-60:root.width)-16,(vertical?icons.height:icons.width)+30)
 property real panelWidth:vertical?104:panelLength
 Behavior on panelLength {NumberAnimation {duration:System.reducedMotion?0:220;easing.type:Easing.OutCubic}}
 function updateInput(){if(typeof Chrome!=="undefined")Chrome.setContentRect(root.x+(vertical?8:(root.width-panelWidth)/2),root.y+(vertical?(root.height-panelLength)/2:8),vertical?104:panelWidth,vertical?panelLength:104)}
 onPanelLengthChanged:updateInput()
 onWidthChanged:updateInput()
 onHeightChanged:updateInput()
 onXChanged:updateInput()
 onYChanged:updateInput()
 ShellGlass {x:root.vertical?(root.edge==="left"?8:27):(root.width-root.panelLength)/2;y:root.vertical?(root.height-root.panelLength)/2:root.edge==="top"?8:27;width:root.vertical?76:root.panelLength;height:root.vertical?root.panelLength:76;radius:25;tint:0.45}
 Flickable {
  id:scroller
  x:root.vertical?8:(root.width-width)/2;y:root.vertical?(root.height-height)/2:8
  width:root.vertical?104:Math.min(root.width-30,icons.width+12);height:root.vertical?Math.min(root.height-60,icons.height+12):104
  contentWidth:root.vertical?width:icons.width+12;contentHeight:root.vertical?icons.height+12:height
  clip:true;interactive:!root.dragging;flickableDirection:root.vertical?Flickable.VerticalFlick:Flickable.HorizontalFlick;boundsBehavior:Flickable.StopAtBounds
  Grid {
   id:icons;x:root.vertical?(root.edge==="left"?13:32):6;y:root.vertical?6:root.edge==="top"?13:31
   columns:root.vertical?1:Math.max(1,dockModel.count);columnSpacing:10;rowSpacing:0
   move:Transition {NumberAnimation {properties:"x,y";duration:System.reducedMotion?0:220;easing.type:Easing.OutCubic}}
   add:Transition {NumberAnimation {properties:"opacity";from:0;to:1;duration:System.reducedMotion?0:180}}
   Repeater {
    id:items;model:dockModel
    Item {
     id:slot
     required property string appKey
     required property string appName
     required property string icon
     required property bool pinned
     required property bool library
     required property int windowId
     required property int index
     width:50;height:62
     readonly property bool running:root.windows.some(w=>root.matches(root.appFor(appKey),w)||(windowId&&w.id===windowId))
     Rectangle {visible:slot.library;x:root.vertical?4:-6;y:root.vertical?-4:5;width:root.vertical?42:1;height:root.vertical?1:42;color:"#526A5380"}
     Button {
      id:button;objectName:"dock-"+slot.appKey;anchors.horizontalCenter:parent.horizontalCenter;width:50;height:50;padding:0;hoverEnabled:true
      opacity:root.dragging&&root.draggedId===slot.appKey?0.25:1
      scale:!System.reducedMotion&&hovered&&!root.dragging?1.2:1
      transformOrigin:Item.Bottom
      Behavior on scale {NumberAnimation {duration:System.reducedMotion?0:140;easing.type:Easing.OutCubic}}
      background:Rectangle {radius:13;color:"transparent";border.color:button.activeFocus?"#E8CAFF":"transparent";border.width:2}
      contentItem:Image {source:slot.icon;sourceSize:Qt.size(128,128);fillMode:Image.PreserveAspectFit}
      onClicked:root.activate(slot.appKey,slot.windowId)
      ShellToolTip {visible:button.hovered&&!root.dragging;delay:350;text:slot.appName}
      Accessible.name:slot.appName
      TapHandler {acceptedButtons:Qt.RightButton;onTapped:{context.appId=slot.appKey;context.windowId=slot.windowId;context.isLibrary=slot.library;context.name=slot.appName;context.popup()}}
      DragHandler {
       id:drag;target:null;enabled:slot.pinned;acceptedButtons:Qt.LeftButton
       onActiveChanged:{
        if(active){root.dragging=true;root.draggedId=slot.appKey;root.dragIcon=slot.icon;root.reorderAt(centroid.scenePosition)}
        else if(root.dragging&&root.draggedId===slot.appKey)root.saveOrder()
       }
       onActiveTranslationChanged:if(active)root.reorderAt(centroid.scenePosition)
      }
     }
     Rectangle {anchors.horizontalCenter:parent.horizontalCenter;y:55;width:4;height:4;radius:2;color:"#E8C9FF";visible:slot.running}
    }
   }
  }
  ScrollBar.horizontal:ScrollBar {policy:root.vertical?ScrollBar.AlwaysOff:ScrollBar.AsNeeded}
  ScrollBar.vertical:ScrollBar {policy:root.vertical?ScrollBar.AsNeeded:ScrollBar.AlwaysOff}
 }
 Image {visible:root.dragging;x:root.dragX-28;y:Math.max(0,root.dragY-28);width:56;height:56;source:root.dragIcon;sourceSize:Qt.size(128,128);z:20}
 ShellMenu {
  id:context;property string appId:"";property string name:"";property int windowId:0;property bool isLibrary:false
  ShellMenuItem {text:"Open "+context.name;onTriggered:root.activate(context.appId,context.windowId)}
  Instantiator {
   model:root.windows.filter(w=>root.matches(root.appFor(context.appId),w))
   delegate:ShellMenuItem {required property var modelData;text:(modelData.minimized?"↑  ":"")+(modelData.title||modelData.name);onTriggered:Windows.activate(modelData.id)}
   onObjectAdded:(index,object)=>context.insertItem(index+1,object)
   onObjectRemoved:(index,object)=>context.removeItem(object)
  }
  ShellMenuItem {visible:!context.isLibrary&&!context.appId.startsWith("running:");text:{let pins=System.pinnedApps;return System.isPinned(context.appId)?"Unpin from Dock":"Pin to Dock"} onTriggered:System.setPinned(context.appId,!System.isPinned(context.appId))}
  ShellMenuItem {visible:System.isPinned(context.appId);text:"Move Left";onTriggered:System.movePinned(context.appId,-1)}
  ShellMenuItem {visible:System.isPinned(context.appId);text:"Move Right";onTriggered:System.movePinned(context.appId,1)}
 }
}
