pragma Singleton
import QtQuick

// Neutral name for the metrics used by the QML shared with Sailfish OS
// (qml-common), which cannot use either platform's own Theme directly.
QtObject {
    readonly property real paddingSmall: Theme.paddingSmall
    readonly property real paddingMedium: Theme.paddingMedium
    readonly property real paddingLarge: Theme.paddingLarge
    readonly property real horizontalPageMargin: Theme.horizontalPageMargin
    readonly property real itemSizeSmall: Theme.itemSizeSmall
    readonly property real itemSizeMedium: Theme.itemSizeMedium
    readonly property real itemSizeLarge: Theme.itemSizeLarge
    readonly property real fontSizeTiny: Theme.fontSizeTiny
    readonly property real fontSizeExtraSmall: Theme.fontSizeExtraSmall
    readonly property real fontSizeSmall: Theme.fontSizeSmall
    readonly property real fontSizeMedium: Theme.fontSizeMedium
    readonly property real fontSizeLarge: Theme.fontSizeLarge
    readonly property color primaryColor: Theme.primaryColor
    readonly property color secondaryColor: Theme.secondaryColor
    readonly property color highlightColor: Theme.highlightColor
    readonly property color secondaryHighlightColor: Theme.secondaryHighlightColor
}
