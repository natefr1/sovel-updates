import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
 id:root;required property string preferenceKey
 property var definition:SettingsService.definitions[preferenceKey]||({})
 property var currentValue:SettingsService.preferences[preferenceKey]
 property bool supported:true
 Layout.fillWidth:true;spacing:8
 RowLayout {Layout.fillWidth:true;spacing:14
  SettingsLabel {text:root.definition.label||root.preferenceKey}
  SettingsToggle {visible:root.definition.kind==="switch";checked:!!root.currentValue;enabled:root.supported;Accessible.name:root.definition.label||"Preference";onClicked:SettingsService.setPreference(root.preferenceKey,checked)}
  SovelCombo {id:choice;visible:root.definition.kind==="choice";Layout.preferredWidth:190;enabled:root.supported
   model:root.definition.labels||[];currentIndex:Math.max(0,(root.definition.choices||[]).indexOf(root.currentValue));onActivated:SettingsService.setPreference(root.preferenceKey,root.definition.choices[currentIndex])
   contentItem:Text {text:choice.displayText;color:SettingsService.colors.text;elide:Text.ElideRight;verticalAlignment:Text.AlignVCenter}
   background:Rectangle {radius:9;color:SettingsService.colors.field;border.color:SettingsService.colors.border}
  }
  TextField {id:field;visible:root.definition.kind==="text"||root.definition.kind==="color";Layout.preferredWidth:190;enabled:root.supported;maximumLength:root.definition.maxLength||80;selectByMouse:true;color:SettingsService.colors.text
   text:root.currentValue===undefined?"":String(root.currentValue);onEditingFinished:if(text!==String(root.currentValue))SettingsService.setPreference(root.preferenceKey,text)
   background:Rectangle {radius:9;color:SettingsService.colors.field;border.color:field.activeFocus?SettingsService.colors.accent:SettingsService.colors.border}
  }
 }
 RowLayout {visible:root.definition.kind==="slider";Layout.fillWidth:true
  Slider {id:slider;Layout.fillWidth:true;from:root.definition.minimum||0;to:root.definition.maximum||100;stepSize:root.definition.step||1;enabled:root.supported
   property bool dirty:false;property real requestedValue:0
   Binding {target:slider;property:"value";value:Number(root.currentValue||0);when:!slider.pressed;restoreMode:Binding.RestoreNone}
   onMoved:{requestedValue=value;if(pressed)dirty=true;else SettingsService.setPreference(root.preferenceKey,Math.round(value))}
   onPressedChanged:if(!pressed&&dirty){dirty=false;SettingsService.setPreference(root.preferenceKey,Math.round(requestedValue))}
   Accessible.name:root.definition.label||"Preference"
   background:Rectangle {x:slider.leftPadding;y:slider.topPadding+slider.availableHeight/2-2;width:slider.availableWidth;height:4;radius:2;color:SettingsService.colors.border
    Rectangle {width:parent.width*slider.visualPosition;height:4;radius:2;color:SettingsService.colors.accent}
   }
   handle:Rectangle {x:slider.leftPadding+slider.visualPosition*(slider.availableWidth-width);y:slider.topPadding+slider.availableHeight/2-height/2;width:18;height:18;radius:9;color:SettingsService.colors.accent;border.color:SettingsService.colors.accentText}
  }
  SettingsLabel {Layout.fillWidth:false;Layout.preferredWidth:85;horizontalAlignment:Text.AlignRight;text:Math.round(slider.value)+(root.definition.unit||"");color:SettingsService.colors.muted}
 }
}
