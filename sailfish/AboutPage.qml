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
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge
        VerticalScrollDecorator { }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("About Snapszer") }

            Image {
                width: Theme.itemSizeHuge
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                source: Qt.resolvedUrl("icons/icon-256.png")
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            Label {
                width: parent.width
                text: "Snapszer"
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeHuge
                font.bold: true
                color: Theme.highlightColor
            }

            Label {
                width: parent.width
                text: qsTr("Classic Hungarian 66")
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryHighlightColor
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("A native Sailfish OS edition of the classic two-player Snapszer, also known as Snapszli or 66.")
            }

            SectionHeader { text: qsTr("Included") }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Theme.secondaryColor
                text: qsTr("• Classic two-player Snapszer / Snapszli / 66\n" +
                           "• Four computer difficulty levels\n" +
                           "• Piatnik and Betyár Hungarian card designs\n" +
                           "• 20/40 declarations, trump exchange and talon closing\n" +
                           "• Match scoring to 7 game points\n" +
                           "• Animated or immediate card movement\n" +
                           "• English and Hungarian interface\n" +
                           "• Automatic recovery of an unfinished match")
            }

            SectionHeader { text: qsTr("Release") }

            DetailItem { label: qsTr("Version"); value: "1.0" }
            DetailItem { label: qsTr("Developer"); value: "edp17" }
            DetailItem { label: qsTr("License"); value: "MIT" }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("View source on GitHub")
                onClicked: Qt.openUrlExternally("https://github.com/edp17/harbour-snapszer")
            }

            Label {
                width: parent.width - 2 * Theme.horizontalPageMargin
                x: Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Made for players who want the classic Hungarian 66-card-table experience on Sailfish OS. This independent application is not affiliated with any card publisher.")
            }
        }
    }
}
