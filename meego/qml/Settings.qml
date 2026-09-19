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
import QtQuick 1.1
import com.nokia.meego 1.0
import "Style.js" as Style

Page {
    id: page


    tools: ToolBarLayout {
        ToolIcon {
            iconId: "toolbar-back"
            onClicked: pageStack.pop()
        }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        contentHeight: content.height + Style.paddingLarge
        flickableDirection: Flickable.VerticalFlick
        clip: true

        Column {
            id: content
            width: parent.width
            spacing: Style.paddingMedium

            PageHeader {
                title: qsTr("Settings")
                description: qsTr("Players, cards and movement")
            }

            SectionHeader { text: qsTr("Players") }

            Text {
                x: Style.horizontalPageMargin
                text: qsTr("Your name")
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }
            TextField {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                placeholderText: qsTr("Player")
                text: snapszerEngine.playerName
                onTextChanged: snapszerEngine.playerName = text
            }

            Text {
                x: Style.horizontalPageMargin
                text: qsTr("Opponent name")
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }
            TextField {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                placeholderText: qsTr("AI")
                text: snapszerEngine.opponentName
                onTextChanged: snapszerEngine.opponentName = text
            }

            SectionHeader { text: qsTr("Cards") }

            SelectionButton {
                label: qsTr("Card style")
                options: ["Piatnik", "Betyár"]
                currentIndex: snapszerEngine.cardStyle === "Betyar" ? 1 : 0
                onSelected: snapszerEngine.cardStyle = index === 1 ? "Betyar" : "Piatnik"
            }

            SectionHeader { text: qsTr("Opponent") }

            SelectionButton {
                label: qsTr("Difficulty")
                options: [qsTr("Easy"), qsTr("Normal"), qsTr("Hard"), qsTr("Expert")]
                currentIndex: snapszerEngine.aiDifficulty
                onSelected: snapszerEngine.aiDifficulty = index
            }

            Text {
                x: Style.horizontalPageMargin
                text: qsTr("Opponent pace") + ":  "
                      + (paceSlider.value < 400 ? qsTr("Fast") : paceSlider.value < 900 ? qsTr("Normal") : qsTr("Slow"))
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }
            Slider {
                id: paceSlider
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                minimumValue: 150
                maximumValue: 1500
                stepSize: 50
                value: snapszerEngine.aiPlayDelay
                onValueChanged: snapszerEngine.aiPlayDelay = Math.round(value)
            }

            SectionHeader { text: qsTr("Three and four players") }

            SelectionButton {
                label: qsTr("Rules for 3 players")
                options: [multiEngine.rulesNameFor(3, 0), multiEngine.rulesNameFor(3, 1)]
                currentIndex: multiEngine.rules3
                onSelected: multiEngine.rules3 = index
            }

            SelectionButton {
                label: qsTr("Rules for 4 players")
                options: [multiEngine.rulesNameFor(4, 0), multiEngine.rulesNameFor(4, 1)]
                currentIndex: multiEngine.rules4
                description: qsTr("Applies to the next new match")
                onSelected: multiEngine.rules4 = index
            }

            SectionHeader { text: qsTr("Animation") }

            Item {
                width: parent.width
                height: animationSwitch.height
                Column {
                    x: Style.horizontalPageMargin
                    width: parent.width - 2 * Style.horizontalPageMargin - animationSwitch.width - Style.paddingMedium
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        width: parent.width
                        text: qsTr("Enable animations")
                        color: Style.primaryColor
                        font.pixelSize: Style.fontSizeSmall
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        text: qsTr("Turn off for immediate card movement")
                        color: Style.secondaryColor
                        font.pixelSize: Style.fontSizeExtraSmall
                    }
                }
                Switch {
                    id: animationSwitch
                    anchors.right: parent.right
                    anchors.rightMargin: Style.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    checked: snapszerEngine.animationsEnabled
                    onCheckedChanged: snapszerEngine.animationsEnabled = checked
                }
            }

            Text {
                x: Style.horizontalPageMargin
                text: qsTr("Card speed") + ":  "
                      + (speedSlider.value <= 0.75 ? qsTr("Slow") : speedSlider.value <= 1.25 ? qsTr("Normal") : qsTr("Fast"))
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
                opacity: snapszerEngine.animationsEnabled ? 1 : 0.5
            }
            Slider {
                id: speedSlider
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                minimumValue: 0.5
                maximumValue: 2.0
                stepSize: 0.25
                value: snapszerEngine.animationSpeed
                enabled: snapszerEngine.animationsEnabled
                onValueChanged: snapszerEngine.animationSpeed = value
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Reset to defaults")
                onClicked: {
                    snapszerEngine.playerName = "Player"
                    snapszerEngine.opponentName = "AI"
                    snapszerEngine.cardStyle = "Piatnik"
                    snapszerEngine.aiDifficulty = 1
                    snapszerEngine.aiPlayDelay = 650
                    snapszerEngine.animationsEnabled = true
                    snapszerEngine.animationSpeed = 1.0
                }
            }
        }
    }
    ScrollDecorator { flickableItem: flickable }
}
