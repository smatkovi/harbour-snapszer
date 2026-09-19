/*
    Card of the MeeGo Harmattan edition. Same images and behaviour as the
    Sailfish OS Card.qml, without the graphical-effects shadow (QtQuick 1.1).
*/
import QtQuick 1.1
import "Style.js" as Style

Item {
    id: root
    property bool pressed: false
    property bool faceUp: true
    property string cardStyle: "Piatnik"
    property string cardId
    // No default size: every table sets width/height or fills its slot, and a
    // height binding of our own would fight the fill anchors under Qt 4.

    scale: pressed ? 1.06 : 1.0
    // Lifted with a transform: a y binding would fight the fill anchors of
    // the table (Qt 4 reports an anchor loop).
    transform: Translate {
        y: root.pressed ? -Style.paddingSmall : 0
        Behavior on y {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    Behavior on scale {
        NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
    }

    // A plain offset shadow instead of DropShadow.
    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 3
        anchors.topMargin: 3
        anchors.rightMargin: -3
        anchors.bottomMargin: -3
        radius: 4
        color: "#30000000"
    }

    Image {
        anchors.fill: parent
        source: Qt.resolvedUrl(root.cardStyle === "Betyar"
                               ? "../images/cards/betyar_back.png"
                               : "../images/cards/back.png")
        fillMode: Image.PreserveAspectFit
        smooth: true
        visible: !root.faceUp
    }

    Image {
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        source: root.faceUp && root.cardId !== ""
                ? Qt.resolvedUrl("../images/cards/"
                                 + (root.cardStyle === "Betyar" ? "betyar_card_" : "card-")
                                 + root.cardId + ".png")
                : ""
        smooth: true
        visible: root.faceUp
    }
}
