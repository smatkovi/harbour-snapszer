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

Page {
    id: page
    property string cardStyle: "Piatnik"
    allowedOrientations: Orientation.Portrait

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge
        VerticalScrollDecorator { }

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingSmall

            PageHeader { title: qsTr("How to play") }

            SectionHeader { text: qsTr("Goal and deck") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Two-player Snapszer uses 20 Hungarian cards: Ace, Ten, King, Upper and Lower in all four suits. Reach at least 66 points from tricks and declarations. A match is won by reaching 7 game points.")
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingSmall
                Repeater {
                    model: ["0_14", "0_10", "0_13", "0_12", "0_11"]
                    Card {
                        width: Theme.itemSizeMedium * 0.68
                        height: width * 1.4
                        cardId: modelData
                        cardStyle: page.cardStyle
                    }
                }
            }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Card values: Ace 11, Ten 10, King 4, Upper 3, Lower 2.")
            }

            SectionHeader { text: qsTr("Deal and open talon") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Each player receives five cards. One card is turned face up to set trumps and the other nine form the face-down talon. The non-dealer leads. While the talon is open you may answer with any card. A trick is won by a higher card of the led suit, or by a trump against a non-trump. The trick winner draws first and leads next.")
            }

            SectionHeader { text: qsTr("20 and 40") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("When leading, a King and Upper of the same suit may be declared by playing either card and showing the pair. It scores 20, or 40 in trumps. A declaration counts only after that player has won at least one trick.")
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingLarge
                Card { width: Theme.itemSizeMedium; height: width * 1.4; cardId: "1_13"; cardStyle: page.cardStyle }
                Label { anchors.verticalCenter: parent.verticalCenter; text: "+"; font.pixelSize: Theme.fontSizeLarge }
                Card { width: Theme.itemSizeMedium; height: width * 1.4; cardId: "1_12"; cardStyle: page.cardStyle }
            }

            SectionHeader { text: qsTr("Trump Lower exchange") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("When you are on lead and at least three face-down talon cards still cover the trump indicator, you may exchange the trump Lower from your hand for the visible trump card.")
            }

            SectionHeader { text: qsTr("Closing the talon") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("On lead you may close the talon. Nobody draws afterwards and strict play begins immediately. You must follow suit and overtake if possible; otherwise play the led suit, then trump if void, and only then discard another suit. Closing is a commitment: if the closer fails to make 66, the opponent wins extra game points.")
            }

            SectionHeader { text: qsTr("Calling 66") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("After winning a trick, call 66 once your trick points plus valid declarations reach at least 66. Normally the round is worth 1 game point if the opponent has at least 33, 2 if below 33, or 3 if the opponent has taken no trick. If nobody calls 66 and the open talon is played to the end, the last trick wins 1 game point.")
            }

            SectionHeader { text: qsTr("Current scope") }
            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("This release implements classic two-player Snapszer/66. The separate opening ‘Snapszer’ contract, kontra/rekontra and three- or four-player bidding variants are not included yet.")
            }
        }
    }
}
