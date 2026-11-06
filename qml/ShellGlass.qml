import QtQuick
Item {
 id:root
 property real radius:18
 property real tint:0.50
 readonly property bool light:typeof SettingsService!=="undefined"&&!!SettingsService&&SettingsService.colors.light
 property bool lab:typeof System!=="undefined"&&!!System&&System.privateMode
 readonly property bool simple:typeof System!=="undefined"&&!!System&&System.lowEffects
 // Current Labwc has no compositor blur. This is a lit translucent material:
 // never a copy of the wallpaper pretending to be the content behind it.
 readonly property real alpha:simple?0.98:Math.min(0.98,root.tint+0.20)
 readonly property color topColor:light?Qt.rgba(.97,.97,.99,alpha):lab?Qt.rgba(.075,.045,.115,alpha):Qt.rgba(.135,.145,.175,alpha)
 readonly property color bottomColor:light?Qt.rgba(.90,.91,.94,Math.min(.98,alpha+.05)):lab?Qt.rgba(.020,.014,.040,Math.min(.98,alpha+.05)):Qt.rgba(.075,.082,.102,Math.min(.98,alpha+.05))
 Rectangle {anchors.fill:parent;radius:root.radius;border.width:1
  border.color:root.light?"#33000000":root.lab?"#59B98BFF":"#2EFFFFFF"
  gradient:Gradient {GradientStop {position:0;color:root.topColor} GradientStop {position:1;color:root.bottomColor}}
 }
 // Specular sheen across the upper half, like light on a curved glass edge.
 Rectangle {x:1;y:1;width:Math.max(0,parent.width-2);height:Math.min(parent.height*.5,44);radius:Math.max(0,root.radius-1);visible:!root.simple
  gradient:Gradient {GradientStop {position:0;color:root.light?"#40FFFFFF":root.lab?"#1CE3D9FC":"#1AFFFFFF"} GradientStop {position:1;color:"#00FFFFFF"}}
 }
 Rectangle {x:root.radius;y:1;width:Math.max(0,root.width-2*root.radius);height:1;color:root.light?"#CCFFFFFF":root.lab?"#99E3D9FC":"#73FFFFFF"}
 Rectangle {x:root.radius;y:parent.height-2;width:Math.max(0,root.width-2*root.radius);height:1;visible:!root.simple;color:root.light?"#14000000":"#4D000000"}
}
