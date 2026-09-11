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

    // Same root-context engine instance as Settings.qml uses.
    property var engine: snapszerEngine

    allowedOrientations: Orientation.All

    Component.onCompleted: engine.discoverLanHosts()
    // Leaving this page without a started game cancels hosting or joining.
    Component.onDestruction: {
        if (engine && !engine.networkGame)
            engine.cancelLan()
    }

    Connections {
        target: engine
        onNetworkChanged: {
            if (engine.networkGame && pageStack.currentPage === page)
                pageStack.pop()
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Search again")
                onClicked: engine.discoverLanHosts()
            }
        }

        VerticalScrollDecorator { }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("LAN game")
                description: qsTr("Against another phone in the same network")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Both phones need Snapszer and must be connected to the same Wi-Fi network; a hotspot opened by one of the phones works too. Your game against the AI is kept and continues afterwards.")
            }

            Item {
                width: parent.width
                height: statusRow.height
                visible: engine.networkStatus !== "" || engine.lanBusy

                Row {
                    id: statusRow
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium

                    BusyIndicator {
                        id: busy
                        size: BusyIndicatorSize.Small
                        running: engine.lanBusy
                        visible: running
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        width: parent.width - (busy.visible ? busy.width + parent.spacing : 0)
                        wrapMode: Text.WordWrap
                        text: engine.networkStatus
                        color: Theme.highlightColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: engine.lanBusy
                text: qsTr("Cancel")
                onClicked: engine.cancelLan()
            }

            SectionHeader { text: qsTr("Host a game") }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: !engine.lanBusy
                text: qsTr("Host")
                onClicked: engine.hostLanGame()
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: engine.localAddresses !== ""
                      ? qsTr("Address of this phone: %1").arg(engine.localAddresses)
                      : qsTr("This phone is not connected to a network")
            }

            SectionHeader { text: qsTr("Join a game") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: engine.discoveredHosts.length === 0
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: qsTr("No hosted games found yet. Start hosting on the other phone, then pull down to search again.")
            }

            Repeater {
                model: engine.discoveredHosts
                BackgroundItem {
                    width: content.width
                    height: Theme.itemSizeMedium
                    enabled: !engine.lanBusy
                    onClicked: engine.joinLanGame(modelData.address)

                    Column {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        Label {
                            width: parent.width
                            text: modelData.name
                            truncationMode: TruncationMode.Fade
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                        }
                        Label {
                            text: modelData.address
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                        }
                    }
                }
            }

            TextField {
                id: addressField
                width: parent.width
                label: qsTr("Address of the hosting phone")
                placeholderText: qsTr("e.g. 192.168.1.23")
                text: engine.lanAddress
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhPreferNumbers
                EnterKey.enabled: text.trim().length > 0 && !engine.lanBusy
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    focus = false
                    engine.joinLanGame(text)
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: addressField.text.trim().length > 0 && !engine.lanBusy
                text: qsTr("Connect")
                onClicked: engine.joinLanGame(addressField.text)
            }
        }
    }
}
