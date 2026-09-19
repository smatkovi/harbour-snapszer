import QtQuick 1.1
import "Style.js" as Style

Item {
    property string text: ""
    width: parent ? parent.width : 480
    height: label.height + Style.paddingMedium

    Text {
        id: label
        anchors.right: parent.right
        anchors.rightMargin: Style.horizontalPageMargin
        anchors.bottom: parent.bottom
        text: parent.text
        color: Style.highlightColor
        font.pixelSize: Style.fontSizeSmall
        font.bold: true
    }
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Style.horizontalPageMargin
        anchors.right: label.left
        anchors.rightMargin: Style.paddingMedium
        anchors.verticalCenter: label.verticalCenter
        height: 1
        color: Style.secondaryHighlightColor
        opacity: 0.5
    }
}
