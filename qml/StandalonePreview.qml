import QtQuick

Item {
 Rectangle {anchors.fill:parent;color:SettingsService.colors.background}
 FilesPreview {anchors.fill:parent;anchors.margins:24;details:previewDetails}
}
