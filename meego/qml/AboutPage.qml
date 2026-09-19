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

            PageHeader { title: qsTr("About Snapszer") }

            Image {
                width: Style.itemSizeHuge
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                source: Qt.resolvedUrl("../icons/icon-256.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Text {
                width: parent.width
                text: "Snapszer"
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Style.fontSizeHuge
                font.bold: true
                color: Style.highlightColor
            }

            Text {
                width: parent.width
                text: qsTr("Classic Hungarian 66")
                horizontalAlignment: Text.AlignHCenter
                color: Style.secondaryHighlightColor
                font.pixelSize: Style.fontSizeMedium
            }

            Text {
                width: parent.width - 2 * Style.horizontalPageMargin
                x: Style.horizontalPageMargin
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
                text: qsTr("The classic two-player Snapszer, also known as Snapszli or 66, for the Nokia N9. Same game, computer opponents and LAN play as the Sailfish OS and Android editions.")
            }

            SectionHeader { text: qsTr("Included") }

            // One entry per line: qsTr() of this Qt only handles Latin-1
            // sources, so the bullets and "Betyár" stay outside the keys.
            Column {
                width: parent.width - 2 * Style.horizontalPageMargin
                x: Style.horizontalPageMargin
                Repeater {
                    model: [
                        qsTr("Classic two-player Snapszer / Snapszli / 66"),
                        qsTr("Four computer difficulty levels"),
                        qsTr("Piatnik and %1 Hungarian card designs").arg("Betyár"),
                        qsTr("20/40 declarations, trump exchange and talon closing"),
                        qsTr("Match scoring to 7 game points"),
                        qsTr("Animated or immediate card movement"),
                        qsTr("English and Hungarian interface"),
                        qsTr("Automatic recovery of an unfinished match")
                    ]
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        color: Style.secondaryColor
                        font.pixelSize: Style.fontSizeSmall
                        text: "• " + modelData
                    }
                }
            }

            SectionHeader { text: qsTr("Release") }

            Repeater {
                model: [
                    { "label": qsTr("Version"), "value": "1.1.0" },
                    { "label": qsTr("Developer"), "value": "edp17" },
                    { "label": qsTr("License"), "value": "MIT" }
                ]
                Item {
                    width: content.width
                    height: Style.fontSizeSmall + Style.paddingSmall
                    Text {
                        anchors.right: parent.horizontalCenter
                        anchors.rightMargin: Style.paddingMedium
                        text: modelData.label
                        color: Style.secondaryHighlightColor
                        font.pixelSize: Style.fontSizeSmall
                    }
                    Text {
                        anchors.left: parent.horizontalCenter
                        anchors.leftMargin: Style.paddingMedium
                        text: modelData.value
                        color: Style.primaryColor
                        font.pixelSize: Style.fontSizeSmall
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("View source on GitHub")
                onClicked: Qt.openUrlExternally("https://github.com/smatkovi/harbour-snapszer")
            }

            Text {
                width: parent.width - 2 * Style.horizontalPageMargin
                x: Style.horizontalPageMargin
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Style.secondaryColor
                font.pixelSize: Style.fontSizeExtraSmall
                text: qsTr("Made for players who want the classic Hungarian 66-card-table experience on Sailfish OS. This independent application is not affiliated with any card publisher.")
            }
        }
    }
    ScrollDecorator { flickableItem: flickable }
}
