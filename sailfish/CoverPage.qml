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

CoverBackground {
    property string playerName: "Player"
    property string opponentName: "AI"
    property int playerGamePoints: 0
    property int opponentGamePoints: 0
    property int playerPoints: 0
    property int opponentPoints: 0
    property string cardStyle: "Piatnik"
    property var trumpCard: ({})
    property bool hasTrumpCard: false
    property bool talonClosed: false

    Label {
        id: title
        anchors.top: parent.top
        anchors.topMargin: Theme.paddingLarge
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Snapszer")
        font.pixelSize: Theme.fontSizeLarge
        color: Theme.highlightColor
    }

    Card {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -Theme.paddingSmall
        width: Theme.itemSizeLarge * 0.72
        height: width * 1.4
        cardId: hasTrumpCard && trumpCard.id ? trumpCard.id : "0_14"
        cardStyle: parent.cardStyle
        faceUp: true
        rotation: talonClosed ? 90 : -8
        opacity: 0.92
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.paddingLarge
        spacing: 0

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: playerName + "  " + playerGamePoints + " : " + opponentGamePoints + "  " + opponentName
            font.pixelSize: Theme.fontSizeSmall
            truncationMode: TruncationMode.Fade
        }
        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Round points %1 : %2").arg(playerPoints).arg(opponentPoints)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }
    }
}
