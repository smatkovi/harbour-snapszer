pragma Singleton
import QtQuick

// Silica-like metrics for the Android build. Values are the Sailfish defaults
// at a 540 px reference width, scaled to the device, so the shared table
// layout keeps the proportions it has on Sailfish OS.
QtObject {
    readonly property real screenWidth: Qt.application.screens.length > 0
        ? Math.min(Qt.application.screens[0].width, Qt.application.screens[0].height) : 400
    readonly property real ratio: screenWidth / 540

    readonly property real paddingSmall: 6 * ratio
    readonly property real paddingMedium: 12 * ratio
    readonly property real paddingLarge: 24 * ratio
    readonly property real horizontalPageMargin: 24 * ratio

    readonly property real itemSizeExtraSmall: 70 * ratio
    readonly property real itemSizeSmall: 80 * ratio
    readonly property real itemSizeMedium: 100 * ratio
    readonly property real itemSizeLarge: 110 * ratio
    readonly property real itemSizeExtraLarge: 135 * ratio
    readonly property real itemSizeHuge: 180 * ratio

    readonly property real fontSizeTiny: 16 * ratio
    readonly property real fontSizeExtraSmall: 20 * ratio
    readonly property real fontSizeSmall: 24 * ratio
    readonly property real fontSizeMedium: 28 * ratio
    readonly property real fontSizeLarge: 36 * ratio
    readonly property real fontSizeExtraLarge: 44 * ratio
    readonly property real fontSizeHuge: 60 * ratio

    readonly property color primaryColor: "#ffffff"
    readonly property color secondaryColor: "#b3ffffff"
    readonly property color highlightColor: "#ffd27a"
    readonly property color secondaryHighlightColor: "#c9ad6a"
    readonly property color tableColor: "#0b5d36"
    readonly property color barColor: "#084a2b"
}
