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

    // Same root-context instances as Settings.qml uses.
    property var engine: snapszerEngine
    property var multi: multiEngine
    // Which engine the last host or join went to, for the status line.
    property bool useMulti: false
    readonly property bool busy: engine.lanBusy || multi.lanBusy
    readonly property string statusText: useMulti ? multi.networkStatus : engine.networkStatus

    allowedOrientations: Orientation.All

    // Only one engine hosts or joins at a time; the other one is reset so an
    // old status of it cannot linger.
    function join(address, players) {
        useMulti = players > 2
        if (useMulti) {
            if (!engine.networkGame)
                engine.cancelLan()
            multi.joinLanGame(address)
        } else {
            if (!multi.networkGame)
                multi.cancelLan()
            engine.joinLanGame(address)
        }
    }

    function host(players) {
        useMulti = players > 2
        if (useMulti) {
            if (!engine.networkGame)
                engine.cancelLan()
            multi.hostLanGame(players)
        } else {
            if (!multi.networkGame)
                multi.cancelLan()
            engine.hostLanGame()
        }
    }

    Component.onCompleted: lanBrowser.search()
    // Leaving this page without a started game cancels hosting or joining.
    Component.onDestruction: {
        if (engine && !engine.networkGame)
            engine.cancelLan()
        if (multi && !multi.networkGame)
            multi.cancelLan()
    }

    // A typed address is joined as a two-player game first; a host with a
    // bigger table answers with its size and the join is repeated.
    Connections {
        target: engine
        onLanRedirect: page.join(address, players)
    }
    Connections {
        target: multi
        onLanRedirect: page.join(address, players)
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
                onClicked: lanBrowser.search()
            }
        }

        VerticalScrollDecorator { }

        Column {
            id: content
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("LAN game")
                description: qsTr("Against other phones in the same network")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("All phones need Snapszer and must be connected to the same Wi-Fi network; a hotspot opened by one of the phones works too. Your games against the computer are kept and continue afterwards.")
            }

            Item {
                width: parent.width
                height: statusRow.height
                visible: page.statusText !== "" || page.busy

                Row {
                    id: statusRow
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    spacing: Theme.paddingMedium

                    BusyIndicator {
                        id: busyIndicator
                        size: BusyIndicatorSize.Small
                        running: page.busy
                        visible: running
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        width: parent.width - (busyIndicator.visible ? busyIndicator.width + parent.spacing : 0)
                        wrapMode: Text.WordWrap
                        text: page.statusText
                        color: Theme.highlightColor
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Repeater {
                model: page.multi.lanBusy ? page.multi.lobby : 0
                Label {
                    x: Theme.horizontalPageMargin * 2
                    width: content.width - 3 * Theme.horizontalPageMargin
                    truncationMode: TruncationMode.Fade
                    text: qsTr("Seat %1: %2").arg(index + 1)
                          .arg(modelData.taken ? modelData.name : qsTr("free (computer)"))
                    color: modelData.taken ? Theme.primaryColor : Theme.secondaryColor
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.multi.lanHosting
                text: qsTr("Start game")
                onClicked: page.multi.startLanMatch()
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.busy
                text: qsTr("Cancel")
                onClicked: {
                    page.engine.cancelLan()
                    page.multi.cancelLan()
                }
            }

            SectionHeader { text: qsTr("Host a game") }

            ComboBox {
                id: playersBox
                width: parent.width
                label: qsTr("Players")
                enabled: !page.busy
                currentIndex: 0
                menu: ContextMenu {
                    MenuItem { text: "2" }
                    MenuItem { text: "3" }
                    MenuItem { text: "4" }
                }
                description: currentIndex === 0 ? qsTr("Classic Snapszer")
                             : page.multi.rulesNameFor(currentIndex + 2,
                                                       currentIndex === 1 ? page.multi.rules3 : page.multi.rules4)
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: !page.busy
                text: qsTr("Host")
                onClicked: page.host(playersBox.currentIndex + 2)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: lanBrowser.localAddresses !== ""
                      ? qsTr("Address of this phone: %1").arg(lanBrowser.localAddresses)
                      : qsTr("This phone is not connected to a network")
            }

            BackgroundItem {
                width: parent.width
                height: internetColumn.height + Theme.paddingMedium
                visible: lanBrowser.internetAddresses !== ""
                onClicked: lanBrowser.copyToClipboard(lanBrowser.internetAddresses.split("\n")[0])

                Column {
                    id: internetColumn
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        text: qsTr("For play over the internet (IPv6), tap to copy:")
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                        text: lanBrowser.internetAddresses.split("\n")[0]
                    }
                    Label {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        font.pixelSize: Theme.fontSizeTiny
                        color: Theme.secondaryColor
                        text: qsTr("Works when both networks support IPv6 and allow incoming connections. Over IPv4 the host must be reachable by other means, e.g. a VPN.")
                    }
                }
            }

            SectionHeader { text: qsTr("Join a game") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                visible: lanBrowser.hosts.length === 0
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryColor
                text: lanBrowser.searching ? qsTr("Searching…")
                                           : qsTr("No hosted games found yet. Start hosting on the other phone, then pull down to search again.")
            }

            Repeater {
                model: lanBrowser.hosts
                BackgroundItem {
                    width: content.width
                    height: Theme.itemSizeMedium
                    enabled: !page.busy
                    onClicked: page.join(modelData.address, modelData.players)

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
                            text: qsTr("%1 players · %2 free · %3").arg(modelData.players)
                                  .arg(modelData.openSeats).arg(modelData.address)
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
                placeholderText: qsTr("e.g. 192.168.1.23 or an IPv6 address")
                text: page.engine.lanAddress
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                EnterKey.enabled: text.trim().length > 0 && !page.busy
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    focus = false
                    page.join(text, 2)
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: addressField.text.trim().length > 0 && !page.busy
                text: qsTr("Connect")
                onClicked: page.join(addressField.text, 2)
            }
        }
    }
}
