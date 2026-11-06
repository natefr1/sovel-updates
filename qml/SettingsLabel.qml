import QtQuick
import QtQuick.Layouts
Text {
 Layout.fillWidth:true
 color:SettingsService.colors.text
 font.pixelSize:Number(SettingsService.preferences["appearance.textSize"]||13)
 font.family:SettingsService.preferences["appearance.font"]||"Noto Sans"
 wrapMode:Text.Wrap;textFormat:Text.PlainText
}
