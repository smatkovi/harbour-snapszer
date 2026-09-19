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

    // Same root-context instances as Settings.qml uses.
    property QtObject engine: snapszerEngine
    property QtObject multi: multiEngine
    // Which engine the last host or join went to, for the status line.
    property bool useMulti: false
    property bool busy: engine.lanBusy || multi.lanBusy
    property string statusText: useMulti ? multi.networkStatus : engine.networkStatus

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

    tools: ToolBarLayout {
        ToolIcon {
            iconId: "toolbar-back"
            onClicked: pageStack.pop()
        }
        ToolIcon {
            iconId: "toolbar-refresh"
            onClicked: lanBrowser.search()
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
                title: qsTr("LAN game")
                description: qsTr("Against other phones in the same network")
            }

            Text {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Style.fontSizeExtraSmall
                color: Style.secondaryHighlightColor
                text: qsTr("All phones need Snapszer and must be connected to the same Wi-Fi network; a hotspot opened by one of the phones works too. Your games against the computer are kept and continue afterwards.")
            }

            Item {
                width: parent.width
                height: statusRow.height
                visible: page.statusText !== "" || page.busy

                Row {
                    id: statusRow
                    x: Style.horizontalPageMargin
                    width: parent.width - 2 * Style.horizontalPageMargin
                    spacing: Style.paddingMedium

                    BusyIndicator {
                        id: busyIndicator
                        running: page.busy
                        visible: running
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        width: parent.width - (busyIndicator.visible ? busyIndicator.width + parent.spacing : 0)
                        wrapMode: Text.WordWrap
                        text: page.statusText
                        color: Style.highlightColor
                        font.pixelSize: Style.fontSizeSmall
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            Repeater {
                model: page.multi.lanBusy ? page.multi.lobby : 0
                Text {
                    x: Style.horizontalPageMargin * 2
                    width: content.width - 3 * Style.horizontalPageMargin
                    elide: Text.ElideRight
                    text: qsTr("Seat %1: %2").arg(index + 1)
                          .arg(modelData.taken ? modelData.name : qsTr("free (computer)"))
                    color: modelData.taken ? Style.primaryColor : Style.secondaryColor
                    font.pixelSize: Style.fontSizeSmall
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

            Text {
                x: Style.horizontalPageMargin
                text: qsTr("Players")
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }

            ButtonRow {
                id: playersRow
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                enabled: !page.busy
                property int currentIndex: 0
                Button { text: "2"; onClicked: playersRow.currentIndex = 0 }
                Button { text: "3"; onClicked: playersRow.currentIndex = 1 }
                Button { text: "4"; onClicked: playersRow.currentIndex = 2 }
            }

            Text {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                wrapMode: Text.WordWrap
                color: Style.secondaryColor
                font.pixelSize: Style.fontSizeExtraSmall
                text: playersRow.currentIndex === 0 ? qsTr("Classic Snapszer")
                      : page.multi.rulesNameFor(playersRow.currentIndex + 2,
                                                playersRow.currentIndex === 1 ? page.multi.rules3 : page.multi.rules4)
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: !page.busy
                text: qsTr("Host")
                onClicked: page.host(playersRow.currentIndex + 2)
            }

            Text {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                wrapMode: Text.WordWrap
                font.pixelSize: Style.fontSizeExtraSmall
                color: Style.secondaryColor
                text: lanBrowser.localAddresses !== ""
                      ? qsTr("Address of this phone: %1").arg(lanBrowser.localAddresses)
                      : qsTr("This phone is not connected to a network")
            }

            Item {
                width: parent.width
                height: internetColumn.height + Style.paddingMedium
                visible: lanBrowser.internetAddresses !== ""

                MouseArea {
                    id: internetMouse
                    anchors.fill: parent
                    onClicked: lanBrowser.copyToClipboard(lanBrowser.internetAddresses.split("\n")[0])
                }
                Column {
                    id: internetColumn
                    x: Style.horizontalPageMargin
                    width: parent.width - 2 * Style.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        font.pixelSize: Style.fontSizeExtraSmall
                        color: Style.secondaryColor
                        text: qsTr("For play over the internet (IPv6), tap to copy:")
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: Style.fontSizeExtraSmall
                        color: internetMouse.pressed ? Style.highlightColor : Style.primaryColor
                        text: lanBrowser.internetAddresses.split("\n")[0]
                    }
                    Text {
                        width: parent.width
                        wrapMode: Text.WordWrap
                        font.pixelSize: Style.fontSizeTiny
                        color: Style.secondaryColor
                        text: qsTr("Works when both networks support IPv6 and allow incoming connections. Over IPv4 the host must be reachable by other means, e.g. a VPN.")
                    }
                }
            }

            SectionHeader { text: qsTr("Join a game") }

            Text {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                visible: lanBrowser.hosts.length === 0
                wrapMode: Text.WordWrap
                font.pixelSize: Style.fontSizeSmall
                color: Style.secondaryColor
                text: lanBrowser.searching ? qsTr("Searching") + "…"
                                           : qsTr("No hosted games found yet. Start hosting on the other phone, then tap the refresh icon to search again.")
            }

            Repeater {
                model: lanBrowser.hosts
                Item {
                    width: content.width
                    height: Style.itemSizeMedium

                    Rectangle {
                        anchors.fill: parent
                        color: "#33ffffff"
                        visible: hostMouse.pressed
                    }
                    MouseArea {
                        id: hostMouse
                        anchors.fill: parent
                        enabled: !page.busy
                        onClicked: page.join(modelData.address, modelData.players)
                    }
                    Column {
                        x: Style.horizontalPageMargin
                        width: parent.width - 2 * Style.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            width: parent.width
                            text: modelData.name
                            elide: Text.ElideRight
                            font.pixelSize: Style.fontSizeMedium
                            color: hostMouse.pressed ? Style.highlightColor : Style.primaryColor
                        }
                        Text {
                            text: qsTr("%1 players, %2 free").arg(modelData.players)
                                  .arg(modelData.openSeats) + " · " + modelData.address
                            font.pixelSize: Style.fontSizeExtraSmall
                            color: Style.secondaryColor
                        }
                    }
                }
            }

            Text {
                x: Style.horizontalPageMargin
                text: qsTr("Address of the hosting phone")
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }

            TextField {
                id: addressField
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                placeholderText: qsTr("e.g. 192.168.1.23 or an IPv6 address")
                text: page.engine.lanAddress
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                Keys.onReturnPressed: {
                    if (text.trim().length > 0 && !page.busy) {
                        platformCloseSoftwareInputPanel()
                        page.join(text, 2)
                    }
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
    ScrollDecorator { flickableItem: flickable }
}
