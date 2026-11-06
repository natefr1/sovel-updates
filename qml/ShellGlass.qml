import QtQuick
Item {
 id:root
 property real radius:18
 property real tint:0.50
 readonly property bool light:typeof SettingsService!=="undefined"&&!!SettingsService&&SettingsService.colors.light
 // Current Labwc has no compositor blur. Never substitute a wallpaper texture.
 Rectangle {anchors.fill:parent;radius:root.radius;border.width:1;border.color:"#65DCC4FF"
  gradient:Gradient {
   GradientStop {position:0;color:Qt.rgba(root.light?.96:.20,root.light?.93:.14,root.light?.99:.29,System.lowEffects?0.98:Math.min(0.98,root.tint+0.20))}
   GradientStop {position:1;color:Qt.rgba(root.light?.86:.07,root.light?.82:.04,root.light?.92:.12,System.lowEffects?0.98:Math.min(0.98,root.tint+0.25))}
  }
 }
 Rectangle {x:root.radius;y:1;width:Math.max(0,root.width-2*root.radius);height:1;color:"#8CF5E9FF"}
}
