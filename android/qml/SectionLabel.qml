import QtQuick
import QtQuick.Controls

Label {
    x: Theme.horizontalPageMargin
    width: parent ? parent.width - 2 * Theme.horizontalPageMargin : implicitWidth
    topPadding: Theme.paddingMedium
    horizontalAlignment: Text.AlignRight
    font.pixelSize: Theme.fontSizeSmall
    color: Theme.highlightColor
}
