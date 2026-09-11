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

    // snapszerEngine is a root-context property installed by main.cpp.  This
    // avoids passing a C++ QObject through PageStack, which failed to instantiate
    // Settings.qml on the physical Sailfish device in RC1/RC2.
    property var settings: snapszerEngine

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        VerticalScrollDecorator { }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
                description: qsTr("Players, cards and movement")
            }

            SectionHeader { text: qsTr("Players") }

            TextField {
                width: parent.width
                label: qsTr("Your name")
                placeholderText: qsTr("Player")
                text: settings.playerName
                onTextChanged: settings.playerName = text
            }

            TextField {
                width: parent.width
                label: qsTr("Opponent name")
                placeholderText: qsTr("AI")
                text: settings.opponentName
                onTextChanged: settings.opponentName = text
            }

            SectionHeader { text: qsTr("Cards") }

            ComboBox {
                width: parent.width
                label: qsTr("Card style")
                currentIndex: settings.cardStyle === "Betyar" ? 1 : 0
                menu: ContextMenu {
                    MenuItem { text: "Piatnik" }
                    MenuItem { text: "Betyár" }
                }
                onCurrentIndexChanged: settings.cardStyle = currentIndex === 1 ? "Betyar" : "Piatnik"
            }

            SectionHeader { text: qsTr("Opponent") }

            ComboBox {
                width: parent.width
                label: qsTr("Difficulty")
                currentIndex: settings.aiDifficulty
                menu: ContextMenu {
                    MenuItem { text: qsTr("Easy") }
                    MenuItem { text: qsTr("Normal") }
                    MenuItem { text: qsTr("Hard") }
                    MenuItem { text: qsTr("Expert") }
                }
                onCurrentIndexChanged: settings.aiDifficulty = currentIndex
            }

            Slider {
                width: parent.width
                minimumValue: 150
                maximumValue: 1500
                stepSize: 50
                value: settings.aiPlayDelay
                label: qsTr("Opponent pace")
                valueText: value < 400 ? qsTr("Fast") : value < 900 ? qsTr("Normal") : qsTr("Slow")
                onValueChanged: settings.aiPlayDelay = Math.round(value)
            }

            SectionHeader { text: qsTr("Three and four players") }

            ComboBox {
                width: parent.width
                label: qsTr("Rules for 3 players")
                currentIndex: multiEngine.rules3
                menu: ContextMenu {
                    MenuItem { text: multiEngine.rulesNameFor(3, 0) }
                    MenuItem { text: multiEngine.rulesNameFor(3, 1) }
                }
                onCurrentIndexChanged: multiEngine.rules3 = currentIndex
            }

            ComboBox {
                width: parent.width
                label: qsTr("Rules for 4 players")
                currentIndex: multiEngine.rules4
                description: qsTr("Applies to the next new match")
                menu: ContextMenu {
                    MenuItem { text: multiEngine.rulesNameFor(4, 0) }
                    MenuItem { text: multiEngine.rulesNameFor(4, 1) }
                }
                onCurrentIndexChanged: multiEngine.rules4 = currentIndex
            }

            SectionHeader { text: qsTr("Animation") }

            TextSwitch {
                width: parent.width
                text: qsTr("Enable animations")
                description: qsTr("Turn off for immediate card movement")
                checked: settings.animationsEnabled
                onCheckedChanged: settings.animationsEnabled = checked
            }

            Slider {
                width: parent.width
                minimumValue: 0.5
                maximumValue: 2.0
                stepSize: 0.25
                value: settings.animationSpeed
                label: qsTr("Card speed")
                valueText: value <= 0.75 ? qsTr("Slow")
                           : value <= 1.25 ? qsTr("Normal") : qsTr("Fast")
                enabled: settings.animationsEnabled
                onValueChanged: settings.animationSpeed = value
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Reset to defaults")
                onClicked: {
                    settings.playerName = "Player"
                    settings.opponentName = "AI"
                    settings.cardStyle = "Piatnik"
                    settings.aiDifficulty = 1
                    settings.aiPlayDelay = 650
                    settings.animationsEnabled = true
                    settings.animationSpeed = 1.0
                }
            }
        }
    }
}
