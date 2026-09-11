import QtQuick

Item {
    id: root
    property bool pressed: false
    property bool faceUp: true
    property string cardStyle: "Piatnik"
    property string cardId
    width: Theme.itemSizeLarge * 1.4
    height: width * 1.4

    scale: pressed ? 1.06 : 1.0
    y: pressed ? -Theme.paddingSmall : 0

    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    Behavior on y {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    // Plain rectangle shadow: needs no shader effects, which some Android
    // GPU drivers and offscreen rendering do not handle.
    Rectangle {
        anchors.centerIn: cardImage
        anchors.horizontalCenterOffset: 2
        anchors.verticalCenterOffset: 3
        width: cardImage.paintedWidth
        height: cardImage.paintedHeight
        radius: width * 0.06
        color: "#38000000"
    }

    Image {
        id: cardImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        source: root.faceUp
                ? "qrc:/images/cards/" + (root.cardStyle === "Betyar" ? "betyar_card_" : "card-")
                  + root.cardId + ".png"
                : (root.cardStyle === "Betyar" ? "qrc:/images/cards/betyar_back.png"
                                               : "qrc:/images/cards/back.png")
    }
}
