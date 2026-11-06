import QtQuick
Item {
    id: root
    property real radius: 24
    property real tint: 0.62
    property real bend: 0.025
    property url image: "qrc:/assets/" + (System.wallpaper || "landscape") + ".png"
    property point sampleOffset: Qt.point(x,y)
    property size sceneSize: Qt.size(1280,800)
    Image { id: texture; source: root.image; visible: false; sourceSize: Qt.size(1600,1000) }
    ShaderEffect {
        anchors.fill: parent
        visible: !System.lowEffects && GraphicsInfo.api !== GraphicsInfo.Software
        property variant source: texture
        property vector4d area: Qt.vector4d(root.sampleOffset.x/root.sceneSize.width,root.sampleOffset.y/root.sceneSize.height,root.width/root.sceneSize.width,root.height/root.sceneSize.height)
        property vector2d panelSize: Qt.vector2d(root.width,root.height)
        property real corner: root.radius
        property real tintAmount: root.tint
        property real bendAmount: root.bend
        fragmentShader: "qrc:/shaders/glass.frag.qsb"
    }
    Rectangle { anchors.fill: parent; visible: System.lowEffects || GraphicsInfo.api === GraphicsInfo.Software; radius: root.radius; color: "#EE141021" }
    Rectangle {
        anchors.fill: parent; radius: root.radius; color: "transparent"; border.color: "#557E6AAB"; border.width: 1
        Rectangle { x: parent.radius; y: 0; width: Math.max(0,parent.width-2*parent.radius); height: 1; color: "#70E3D9FC" }
    }
}
