import QtQuick
Item {
    id: root
    property bool animated: false
    property bool motionAllowed: true
    readonly property bool canAnimate: motionAllowed && !System.reducedMotion && !System.lowEffects && visible
    property url image: "qrc:/assets/landscape.png"
    Image { anchors.fill: parent; source: root.image; fillMode: Image.PreserveAspectCrop }
    Rectangle {
        id: aurora; x: parent.width*0.52; y: -parent.height*0.18
        width: parent.width*0.6; height: parent.height*0.75
        radius: height/2; rotation: -28; opacity: 0.13
        gradient: Gradient {
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 0.5; color: "#BF40FA" }
            GradientStop { position: 1; color: "transparent" }
        }
        SequentialAnimation on x {
            running: root.animated && root.canAnimate
            loops: Animation.Infinite
            NumberAnimation { to: root.width*0.32; duration: 15000; easing.type: Easing.InOutSine }
            NumberAnimation { to: root.width*0.52; duration: 15000; easing.type: Easing.InOutSine }
        }
    }
}
