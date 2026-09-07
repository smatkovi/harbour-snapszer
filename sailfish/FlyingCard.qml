/*
    Copyright (C) 2026 edp17 and chatGPT

    This file is part of harbour-snapszer.

    The harbour-snapszer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The harbour-snapszer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the harbour-snapszer. If not, see <http://www.gnu.org/licenses/>.
*/
import QtQuick 2.6
import Sailfish.Silica 1.0

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

    width: Theme.itemSizeLarge * 1.12
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
