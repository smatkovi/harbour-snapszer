import QtQuick 1.1
import "Style.js" as Style

// Harmattan has no page header component; this mirrors the Silica one.
Item {
    property string title: ""
    property string description: ""
    width: parent ? parent.width : 480
    height: titleLabel.height + (descriptionLabel.visible ? descriptionLabel.height : 0) + 2 * Style.paddingLarge

    Text {
        id: titleLabel
        anchors.right: parent.right
        anchors.rightMargin: Style.horizontalPageMargin
        anchors.top: parent.top
        anchors.topMargin: Style.paddingLarge
        text: title
        color: Style.highlightColor
        font.pixelSize: Style.fontSizeLarge
    }
    Text {
        id: descriptionLabel
        anchors.right: parent.right
        anchors.rightMargin: Style.horizontalPageMargin
        anchors.top: titleLabel.bottom
        visible: description !== ""
        text: description
        color: Style.secondaryHighlightColor
        font.pixelSize: Style.fontSizeSmall
    }
}
