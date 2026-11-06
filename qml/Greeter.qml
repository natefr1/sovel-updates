import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
Item {
    id: root
    width: 1280; height: 800
    // Normal greeter (VT 7): the ordinary city sign-in, never listing accounts.
    // Private greeter (own VT): asks only for the private password; no username is shown.
    readonly property bool lab: typeof greeterPrivate !== "undefined" && greeterPrivate
    readonly property string account: typeof privateAccount !== "undefined" ? privateAccount : ""
    readonly property url backdrop: lab ? "qrc:/assets/emblem.png" : "qrc:/assets/city.png"
    property string clock: Qt.formatTime(new Date(),"hh:mm")
    property string date: Qt.formatDate(new Date(),"dddd, MMMM d")
    property string localError: ""
    readonly property color ink: lab ? "#E3D9FC" : "#F2F4F8"
    readonly property color dim: lab ? "#B3A6CC" : "#C9CFDA"
    readonly property color accent: lab ? "#BF40FA" : "#8FA8FF"
    function beginPrivate(){ if(lab && account && Auth.available && (Auth.state==="idle"||Auth.state==="error")) Auth.begin(account) }
    function submit(){
        if(Auth.state === "prompt"){ let value=response.text; response.text=""; Auth.answer(value); value=""; return }
        if(lab){ beginPrivate(); return }
        // The private account signs in only on its own greeter.
        if(account && identity.text.trim() === account){ localError="That account can't sign in here."; identity.selectAll(); return }
        localError=""; Auth.begin(identity.text)
    }
    Wallpaper { anchors.fill: parent; visible: !root.lab; animated: true; image: root.backdrop }
    Rectangle { anchors.fill: parent; visible: !root.lab; color: "#2E0A0F1E" }
    // Private: the emblem sits above the card on matching black, never behind it.
    Rectangle { anchors.fill: parent; visible: root.lab; color: "#040607" }
    Image {
        visible: root.lab; source: root.backdrop; fillMode: Image.PreserveAspectFit; smooth: true
        width: parent.width; height: Math.max(200, login.y - 24); y: 0
        opacity: 0; Component.onCompleted: emblemIn.start()
        NumberAnimation on opacity { id: emblemIn; running: false; to: 1; duration: System.reducedMotion ? 0 : 900; easing.type: Easing.OutCubic }
    }
    Timer { interval: 1000; repeat: true; running: true; onTriggered:{ root.clock=Qt.formatTime(new Date(),"hh:mm");root.date=Qt.formatDate(new Date(),"dddd, MMMM d"); } }
    Component.onCompleted: { entrance.start(); beginPrivate() }
    Text { visible: !Auth.available; anchors.right: parent.right; anchors.rightMargin: 40; y: 30; text: "VISUAL TEST · NOT A LOCK SCREEN"; color: root.dim; font.pixelSize: 10; font.letterSpacing: 2 }
    // Large clock over the city, like an ordinary lock screen.
    Column {
        id: time
        visible: !root.lab
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.max(48, root.height*0.12)
        spacing: 2
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.clock; color: root.ink; font.pixelSize: Math.min(132, root.height*0.15); font.weight: Font.Light; font.letterSpacing: -4 }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.date; color: root.dim; font.pixelSize: 18 }
    }
    Item {
        id: login
        width: Math.min(root.lab ? 460 : 380, root.width-48)
        height: card.implicitHeight + 56
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.lab ? root.height-height-Math.max(36, root.height*0.06) : Math.min(root.height-height-48, time.y+time.height+root.height*0.12)
        opacity: 0
        transform: Translate { id: arrival; y: 18 }
        ParallelAnimation { id: entrance
            NumberAnimation { target: login; property: "opacity"; to: 1; duration: System.reducedMotion ? 0 : 520 }
            NumberAnimation { target: arrival; property: "y"; to: 0; duration: System.reducedMotion ? 0 : 700; easing.type: Easing.OutCubic }
        }
        Glass { anchors.fill: parent; visible: !root.lab; image: root.backdrop; tint: 0.50; tintColor: "#2A0C1220"; radius: 24; sampleOffset: Qt.point(login.x,login.y); sceneSize: Qt.size(root.width,root.height) }
        ShellGlass { anchors.fill: parent; visible: root.lab; lab: true; radius: 24; tint: 0.78 }
        // Thin corner brackets: a restrained instrument frame on the private greeter.
        Repeater {
            model: root.lab ? 4 : 0
            Item {
                required property int index
                width: 18; height: 18
                x: index % 2 ? login.width - width - 10 : 10
                y: index < 2 ? 10 : login.height - height - 10
                Rectangle { width: parent.width; height: 1; color: "#99BF40FA"; y: index < 2 ? 0 : parent.height - 1 }
                Rectangle { width: 1; height: parent.height; color: "#99BF40FA"; x: index % 2 ? parent.width - 1 : 0 }
            }
        }
        ColumnLayout {
            id: card
            x: 28; y: 28; width: parent.width-56; spacing: 12
            Text { visible: root.lab; text: ">> PRIVATE WORKSPACE · " + root.clock; color: root.accent; font.family: "monospace"; font.pixelSize: 12; font.letterSpacing: 1 }
            Text { visible: root.lab; text: ">> identity required"; color: root.dim; font.family: "monospace"; font.pixelSize: 12 }
            Text { visible: !root.lab; text: "Sign in"; color: root.ink; font.pixelSize: 17; font.weight: Font.DemiBold }
            Text {
                Layout.fillWidth: true; wrapMode: Text.Wrap
                text: root.localError || (root.lab ? (Auth.state === "prompt" && Auth.secret ? "> authenticate  " + "•".repeat(Math.min(12,response.text.length))
                      : Auth.state === "idle" ? "> connecting to the private workspace…" : "> " + Auth.prompt) : Auth.prompt)
                color: root.localError || Auth.state === "error" ? "#FFABC8" : root.dim
                font.family: root.lab ? "monospace" : Qt.application.font.family; font.pixelSize: 12
                Accessible.role: Accessible.AlertMessage
            }
            TextField {
                id: identity; Layout.fillWidth: true; Layout.preferredHeight: 42; placeholderText: "Username"; placeholderTextColor: root.dim; color: root.ink; font.pixelSize: 14
                visible: !root.lab && (Auth.state === "idle" || Auth.state === "error")
                enabled: Auth.available; focus: !root.lab
                background: Rectangle { radius: 12; color: "#40000000"; border.color: identity.activeFocus ? root.accent : "#40FFFFFF" }
                onTextEdited: root.localError=""
                onAccepted: root.submit()
                Accessible.name: "Username"
            }
            TextField {
                id: response; Layout.fillWidth: true; Layout.preferredHeight: 42; color: root.ink; visible: Auth.state === "prompt"
                echoMode: Auth.secret ? TextInput.Password : TextInput.Normal
                placeholderText: Auth.secret ? (root.lab ? "Private password" : "Password") : "Response"; placeholderTextColor: root.dim
                font.family: root.lab ? "monospace" : Qt.application.font.family
                background: Rectangle { radius: 12; color: "#59000000"; border.color: root.accent }
                onAccepted: root.submit()
                Accessible.name: Auth.secret ? "Password" : "Authentication response"
            }
            RowLayout {
                spacing: 10
                SovelButton {
                    text: Auth.state === "prompt" ? (root.lab ? "Unlock  →" : "Sign in  →") : root.lab ? "Try again  →" : "Continue  →"; primary: true; tone: root.lab ? "#7829D4" : "#4F6FE0"
                    enabled: Auth.available && (Auth.state === "prompt" || Auth.state === "idle" || Auth.state === "error")
                    onClicked: root.submit()
                }
                SovelButton { text: "Cancel"; visible: !root.lab && Auth.state !== "idle" && Auth.state !== "error"; onClicked:{response.text="";Auth.cancel();} }
                Item { Layout.fillWidth: true }
                SovelButton { visible: root.lab; text: "Return to desktop"; onClicked: { response.text=""; if(Auth.state!=="idle"&&Auth.state!=="error")Auth.cancel(); System.switchToNormalDesktop() } }
            }
        }
    }
    Connections { target: Auth; function onChanged(){ response.text=""; if(Auth.state === "prompt")response.forceActiveFocus(); else if(!root.lab && (Auth.state === "error" || Auth.state === "idle"))identity.forceActiveFocus(); } }
}
