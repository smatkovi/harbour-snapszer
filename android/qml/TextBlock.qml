import QtQuick
import QtQuick.Controls

Label {
    x: Theme.horizontalPageMargin
    width: parent ? parent.width - 2 * Theme.horizontalPageMargin : implicitWidth
    wrapMode: Text.WordWrap
    font.pixelSize: Theme.fontSizeSmall
}
