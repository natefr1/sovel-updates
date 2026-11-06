import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root
    width: 1280; height: 800
    property string clock: Qt.formatTime(new Date(),"hh:mm")
    property string date: Qt.formatDate(new Date(),"dddd, MMMM d")
    Wallpaper { anchors.fill: parent; animated: true; image: "qrc:/assets/lockroom.png" }
    Rectangle { anchors.fill: parent; color: "#55100A1F" }
    Timer { interval: 1000; repeat: true; running: true; onTriggered:{ root.clock=Qt.formatTime(new Date(),"hh:mm");root.date=Qt.formatDate(new Date(),"dddd, MMMM d"); } }
    Image { x: 44; y: 30; width: 40; height: 40; source: "qrc:/assets/icons/sovel.svg"; fillMode: Image.PreserveAspectFit }
    Text { anchors.right: parent.right; anchors.rightMargin: 44; y: 38; text: Auth.available ? "LOCAL AUTHENTICATION" : "GREETER VISUAL TEST · NOT A LOCK SCREEN"; color: "#B8B0CF"; font.pixelSize: 10; font.letterSpacing: 2 }
    Item {
        id: login; width: Math.min(440,root.width-48); height: 472
        x: root.width > 1000 ? root.width*0.10 : (root.width-width)/2
        y: Math.max(100,(root.height-height)/2)
        opacity: 0
        transform: Translate { id: arrival; y: 25 }
        Component.onCompleted: entrance.start()
        ParallelAnimation { id: entrance
            NumberAnimation { target: login; property: "opacity"; to: 1; duration: System.reducedMotion ? 0 : 650 }
            NumberAnimation { target: arrival; property: "y"; to: 0; duration: System.reducedMotion ? 0 : 850; easing.type: Easing.OutCubic }
        }
        Glass { anchors.fill: parent; image: "qrc:/assets/lockroom.png"; tint: 0.44; radius: 30; sampleOffset: Qt.point(login.x,login.y); sceneSize: Qt.size(root.width,root.height) }
        Column {
            x: 34; y: 30; width: parent.width-68; spacing: 14
            Text { text: root.date.toUpperCase(); color: "#CBC1E4"; font.pixelSize: 11; font.letterSpacing: 2 }
            Text { text: root.clock; color: "#F2EDFF"; font.pixelSize: 70; font.weight: Font.Light; font.letterSpacing: -3 }
            Text { text: "Your space. Your system."; color: "#E3D9FC"; font.pixelSize: 18 }
            Rectangle { width: parent.width; height: 1; color: "#40B89AE3" }
            Text { text: ">  identity required"; color: "#C5AAEE"; font.family: "monospace"; font.pixelSize: 12 }
            Text {
                text: Auth.prompt; width: parent.width; wrapMode: Text.Wrap
                color: Auth.state === "error" ? "#FFABC8" : "#B8B0CF"; font.pixelSize: 12
                Accessible.role: Accessible.AlertMessage
            }
            TextField {
                id: identity; width: parent.width; height: 42; placeholderText: "Username"; color: "#F2EDFF"; font.pixelSize: 14
                visible: Auth.state === "idle" || Auth.state === "error"
                enabled: Auth.available; focus: true
                background: Rectangle { radius: 12; color: "#55120920"; border.color: identity.activeFocus ? "#C08BFF" : "#606D5489" }
                onAccepted: Auth.begin(text)
                Accessible.name: "Username"
            }
            TextField {
                id: response; width: parent.width; height: 42; color: "#F2EDFF"; visible: Auth.state === "prompt"
                echoMode: Auth.secret ? TextInput.Password : TextInput.Normal
                placeholderText: Auth.secret ? "Password" : "Response"
                background: Rectangle { radius: 12; color: "#55120920"; border.color: "#C08BFF" }
                onAccepted:{let value=text;text="";Auth.answer(value);value="";}
                Accessible.name: Auth.secret ? "Password" : "Authentication response"
            }
            Row {
                spacing: 12
                SovelButton {
                    text: Auth.state === "prompt" ? "Authenticate  →" : "Continue  →"; primary: true
                    enabled: Auth.available && (Auth.state === "prompt" || Auth.state === "idle" || Auth.state === "error")
                    onClicked: { if(Auth.state === "prompt"){let value=response.text;response.text="";Auth.answer(value);value="";} else Auth.begin(identity.text); }
                }
                SovelButton { text: "Cancel"; visible: Auth.state !== "idle" && Auth.state !== "error"; onClicked:{response.text="";Auth.cancel();} }
            }
        }
    }
    Item {
        visible: root.width > 1000; width: root.width*0.32; height: 400
        x: root.width*0.59; y: (root.height-height)/2
        Glass { anchors.fill: parent; image: "qrc:/assets/lockroom.png"; tint: 0.45; sampleOffset: Qt.point(parent.x,parent.y); sceneSize: Qt.size(root.width,root.height) }
        Text { x: 28; y: 26; text: "S O V E L   /   S P A T I A L"; color: "#C6B1EF"; font.pixelSize: 11; font.letterSpacing: 2 }
        Repeater {
            model: 7
            Rectangle {
                required property int index
                width: 180+index*12; height: 80+index*19; color: "transparent"; border.color: Qt.rgba(.7,.5,1,.12+index*.035)
                x: (parent.width-width)/2; y: 120-index*6; rotation: -18; radius: 16
                transform: Rotation { origin.x: 100; origin.y: 100; axis.x:1; axis.y:0; axis.z:0; angle: 30 }
            }
        }
        Text { x: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 28; width: parent.width-56; wrapMode: Text.Wrap
            text: "A quiet moment before everything begins."; color: "#D7C8EF"; font.pixelSize: 16 }
    }
    Connections { target: Auth; function onChanged(){ response.text="";if(Auth.state === "prompt")response.forceActiveFocus();else if(Auth.state === "error" || Auth.state === "idle")identity.forceActiveFocus(); } }
}
