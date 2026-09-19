/*
    Card in flight between two points of the table (MeeGo edition).
*/
import QtQuick 1.1
import "Style.js" as Style

Item {
    id: root
    property string cardId: ""
    property string cardStyle: "Piatnik"
    property bool faceUp: true
    property real fromX: 0
    property real fromY: 0
    property real toX: 0
    property real toY: 0
    property real fromRotation: 0
    property real toRotation: 0
    property int flightDuration: 320
    property int startDelay: 0
    property bool isFlyingCard: true
    signal finished()

    width: Style.itemSizeLarge * 1.12
    height: width * 1.4
    x: fromX - width / 2
    y: fromY - height / 2
    rotation: fromRotation
    z: 1000

    Card {
        anchors.fill: parent
        cardId: root.cardId
        cardStyle: root.cardStyle
        faceUp: root.faceUp
    }

    SequentialAnimation {
        running: true
        PauseAnimation { duration: root.startDelay }
        ParallelAnimation {
            NumberAnimation {
                target: root; property: "x"
                from: root.fromX - root.width / 2
                to: root.toX - root.width / 2
                duration: root.flightDuration
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root; property: "y"
                from: root.fromY - root.height / 2
                to: root.toY - root.height / 2
                duration: root.flightDuration
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root; property: "rotation"
                from: root.fromRotation; to: root.toRotation
                duration: root.flightDuration
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root; property: "scale"
                from: 0.88; to: 1.0
                duration: root.flightDuration
                easing.type: Easing.OutCubic
            }
        }
        ScriptAction { script: root.finished() }
    }
}
