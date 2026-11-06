import QtQuick
import QtQuick.Controls
Item {
    id: root
    width: 1280; height: 800
    property bool editing: false
    readonly property var localWindows:desktopMode?Windows.entries.filter(window=>!window.outputs||window.outputs.length===0||window.outputs.includes(outputName)):Windows.entries
    readonly property var localActive:localWindows.filter(window=>!window.minimized).reduce(
        (chosen,window)=>!chosen||window.active||(!chosen.active&&window.recent>chosen.recent)?window:chosen,null)
    readonly property bool localMaximized:!!localActive&&localActive.maximized
    readonly property bool identifying:desktopMode&&typeof Displays!=="undefined"&&Displays&&Displays.identifying
    readonly property int displayNumber:identifying?(Displays.identification[outputName]||0):0
    readonly property bool autoHidden: desktopMode && (surfaceKind === "bar" ? !identifying&&localMaximized&&SettingsService.preferences["bar.autoHide"] : surfaceKind === "dock" ? localWindows.some(window=>!window.minimized) : false)
    readonly property bool chromeShown: !desktopMode || Chrome.shown
    onAutoHiddenChanged: Chrome.setAutoHidden(autoHidden)
    readonly property bool primaryOutput: !desktopMode||typeof Displays==="undefined"||!Displays||Displays.primary===outputName
    readonly property var outputConfig: desktopMode&&typeof Displays!=="undefined"&&Displays?Displays.config:({})
    readonly property bool placed: surfaceKind==="bar"&&identifying||primaryOutput||(surfaceKind==="bar"?outputConfig.bar!=="primary":outputConfig.dock!=="primary")
    readonly property string displayWallpaper: desktopMode?(outputConfig.wallpaper||{})[outputName]||"":""
    function configureChrome(){let prefs=Object.assign({},System.shellPreferences);prefs.enabled=root.placed;if(surfaceKind==="bar"){prefs.autoHide=SettingsService.preferences["bar.autoHide"];prefs.enabled=root.placed&&(root.identifying||SettingsService.preferences["bar.visible"])}Chrome.configure(prefs)}
    onPlacedChanged:configureChrome()
    onIdentifyingChanged:configureChrome()
    Connections {target:System;function onPreferencesChanged(){root.configureChrome()}}
    Connections {target:SettingsService;function onChanged(){root.configureChrome()}}
    Connections {target:Windows;function onChanged(){Workspace.setForeground(Windows.activeAppId||"");Workspace.setDesktopActive(!Windows.activeMaximized)}}
    readonly property bool background: surfaceKind === "background" || !desktopMode
    readonly property string widgetOutput: typeof outputName!=="undefined"&&outputName?outputName:"preview"
    function syncWidgets(){
        if(!root.background)return;
        const saved=Workspace.layout;
        for(let i=0;i<saved.length;i++){
            const w=saved[i];let found=-1;
            for(let j=i;j<layoutModel.count;j++)if(layoutModel.get(j).widgetKey===w.id){found=j;break;}
            const position=(w.placements||{})[root.widgetOutput]||w;
            const row={widgetKey:w.id,type:w.type,wx:position.x,wy:position.y,customWidth:position.width||0,customHeight:position.height||0,kindSize:w.size,opacityValue:w.opacity,intervalValue:w.refresh};
            if(found<0)layoutModel.insert(i,row);else {if(found!==i)layoutModel.move(found,i,1);layoutModel.set(i,row);}
        }
        if(layoutModel.count>saved.length)layoutModel.remove(saved.length,layoutModel.count-saved.length);
    }
    Connections {target:Workspace;function onLayoutChanged(){root.syncWidgets()}}
    // Private Mode: the first time two displays are present, the secondary one
    // becomes the command center. Every widget stays movable afterwards.
    property bool privateLayoutChecked:false
    function checkPrivateLayout(){
        if(privateLayoutChecked||!desktopMode||!System.privateMode||surfaceKind!=="background"||!root.primaryOutput||typeof Displays==="undefined"||!Displays)return
        const outputs=Displays.outputs.filter(o=>o.enabled!==false).map(o=>o.name)
        if(outputs.length<2)return
        privateLayoutChecked=true
        Workspace.initializePrivateLayout(outputs,Displays.primary)
    }
    Connections {target:typeof Displays!=="undefined"?Displays:null;function onChanged(){root.checkPrivateLayout()}}
    ListModel {id:layoutModel}
    Wallpaper { anchors.fill: parent; visible: root.background&&(!!root.displayWallpaper||!System.externalWallpaper); image: root.displayWallpaper?"file://"+root.displayWallpaper.split("/").map(encodeURIComponent).join("/"):System.wallpaperUrl }
    MouseArea { anchors.fill: parent; visible: root.background; acceptedButtons: Qt.RightButton; onClicked: root.editing=!root.editing }
    Repeater {
        id:widgets;model:root.background?layoutModel:null
        WorkspaceWidget {
            required property string widgetKey
            required property string type
            required property real wx
            required property real wy
            required property int customWidth
            required property int customHeight
            required property string kindSize
            required property int opacityValue
            required property int intervalValue
            required property int index
            kind:type;widgetId:widgetKey;widgetSize:kindSize;widgetOpacity:opacityValue;refreshSeconds:intervalValue;editing:root.editing
            visible:SettingsService.preferences["widgets.visible"]&&(!desktopMode||!root.localMaximized||root.editing)
            positionX:wx;positionY:wy;savedWidth:customWidth;savedHeight:customHeight
            onPlaced:(x,y,w,h)=>Workspace.placeWidget(widgetId,root.widgetOutput,x,y,w,h)
        }
    }
    Component.onCompleted: {
        root.configureChrome();Chrome.setAutoHidden(root.autoHidden);Workspace.setDesktopActive(!Windows.activeMaximized);
        if(!root.background)return;
        root.syncWidgets();
    }
    Rectangle {
        visible: root.editing; z: 200; anchors.horizontalCenter: parent.horizontalCenter; y: 62; width: editControls.implicitWidth+28; height: 54; radius: 18; color: SettingsService.colors.surface; border.color: SettingsService.colors.border
        Row { anchors.centerIn: parent; spacing: 16
            id:editControls
            Text { text: "Drag widgets or resize their edges"; color: SettingsService.colors.text; anchors.verticalCenter: parent.verticalCenter }
            SovelButton {text:"Undo";onClicked:Workspace.undoWidgetPlacement(root.widgetOutput)}
            SovelButton {text:"Done";primary:true;onClicked:root.editing=false}
        }
    }
    TopBar {
        activeApplication:root.displayNumber?"Display "+root.displayNumber:desktopMode?(root.localActive?root.localActive.name:"Desktop"):Windows.activeName
        width: parent.width; height: 30; visible: !desktopMode || surfaceKind === "bar"&&root.placed
        y: root.chromeShown ? 0 : -30
        Behavior on y { NumberAnimation { duration: System.reducedMotion ? 0 : System.shellPreferences.animationSpeed; easing.type: Easing.OutCubic } }
    }
    Dock {
        id:bottomDock
        windows:root.localWindows
        width: vertical?112:parent.width; height: vertical?parent.height:112
        x:vertical?(edge==="left"?(root.chromeShown?0:-112):(root.chromeShown?parent.width-112:parent.width)):0
        y:vertical?0:edge==="top"?(root.chromeShown?30:-112):(root.chromeShown?parent.height-112:parent.height)
        visible: !desktopMode || surfaceKind === "dock"&&root.placed
        Behavior on x {NumberAnimation {duration:System.reducedMotion?0:System.shellPreferences.animationSpeed;easing.type:Easing.OutCubic}}
        Behavior on y {NumberAnimation {duration:System.reducedMotion?0:System.shellPreferences.animationSpeed;easing.type:Easing.OutCubic}}
    }
    Rectangle {
        visible: root.background&&root.displayNumber>0
        anchors.centerIn:parent
        width:Math.min(380,parent.width-24);height:106;radius:20;z:900
        color:SettingsService.colors.surface;border.color:SettingsService.colors.accent
        Text {anchors.centerIn:parent;color:SettingsService.colors.text;font.pixelSize:24;text:root.displayNumber+" · "+outputName+(root.primaryOutput?" · Primary":"")}
    }
    Rectangle {
        id: toast; z: 1000; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 130
        width: Math.min(600,root.width-40); height: msg.implicitHeight+30; radius: 16; color: "#F0241639"; border.color: "#907D589E"; visible: false
        Text { id:msg; anchors.fill: parent; anchors.margins: 15; text: System.notice; color: "#F2EDFF"; wrapMode: Text.Wrap; horizontalAlignment: Text.AlignHCenter }
        Timer { id: dismiss; interval: 4000; onTriggered: toast.visible=false }
        Connections { target: System; function onNoticeChanged(){if(root.background){toast.visible=true;dismiss.restart();}} }
    }
}
