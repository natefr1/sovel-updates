import QtQuick
import QtQuick.Controls
ComboBox {
 id:root
 readonly property bool themed:typeof SettingsService!=="undefined"&&!!SettingsService
 implicitHeight:38
 // Wide enough for its longest entry, so short combos (Scale) never truncate.
 readonly property real widestEntry:{let widest=0;const items=model&&typeof model!=="number"&&model.length!==undefined?model:[];for(let i=0;i<items.length&&i<200;i++){const item=items[i];widest=Math.max(widest,metrics.advanceWidth(textRole&&item?String(item[textRole]):String(item)))}return widest}
 implicitWidth:Math.max(86,Math.min(360,widestEntry+46))
 FontMetrics {id:metrics;font.pixelSize:14}
 padding:10;rightPadding:26
 background:Rectangle {radius:9;color:root.themed?SettingsService.colors.field:"#392A49";border.color:root.activeFocus?"#DAB6FF":"#70527E"}
 contentItem:Text {text:root.displayText;color:root.themed?SettingsService.colors.text:"#F7EDFF";verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight;textFormat:Text.PlainText}
 indicator:Text {text:"⌄";color:"#DAB6FF";x:root.width-22;y:(root.height-height)/2}
 delegate:ItemDelegate {required property var modelData;required property int index;width:comboPopup.availableWidth;highlighted:root.highlightedIndex===index;contentItem:Text {text:root.textRole?modelData[root.textRole]:modelData;color:root.themed?SettingsService.colors.text:"#F7EDFF";elide:Text.ElideRight;textFormat:Text.PlainText} background:Rectangle {radius:7;color:parent.highlighted?"#775199":"transparent"}}
 popup:Popup {id:comboPopup;y:root.height+4;width:Math.max(root.width,Math.min(360,root.widestEntry+44));padding:6;margins:8;height:Math.min(280,comboList.contentHeight+12);closePolicy:Popup.CloseOnEscape|Popup.CloseOnPressOutside
  background:ShellGlass {radius:12;tint:0.7}
  contentItem:ListView {id:comboList;clip:true;implicitHeight:contentHeight;model:root.popup.visible?root.delegateModel:null;currentIndex:root.highlightedIndex;ScrollIndicator.vertical:ScrollIndicator {}}
 }
}
