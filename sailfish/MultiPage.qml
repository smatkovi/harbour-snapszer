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
    objectName: "multiPage"
    allowedOrientations: Orientation.Portrait

    property var engine: multiEngine

    // A LAN table is left with "Leave LAN game", so a swipe cannot strand the
    // seat of a player who is still connected.
    backNavigation: !engine.networkGame

    onStatusChanged: {
        engine.paused = status !== PageStatus.Active
        if (status === PageStatus.Active && !engine.active)
            closeTimer.restart()
    }

    Connections {
        target: page.engine
        // A LAN match that ended (host left, connection lost) closes the table.
        onStateChanged: {
            if (!page.engine.active)
                closeTimer.restart()
        }
    }

    Timer {
        id: closeTimer
        interval: 50
        onTriggered: {
            if (pageStack.busy)
                restart()
            else if (pageStack.currentPage === page && !page.engine.active)
                pageStack.pop()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#0b5d36"
    }

    SilicaFlickable {
        id: flickable
        anchors.fill: parent
        contentHeight: height

        PullDownMenu {
            MenuItem {
                text: qsTr("Leave LAN game")
                visible: page.engine.networkGame
                onClicked: remorse.execute(qsTr("Leaving the LAN game"), function() { page.engine.cancelLan() })
            }
            MenuItem {
                text: qsTr("New match")
                visible: !page.engine.lanGuest
                onClicked: remorse.execute(qsTr("Starting a new match"), function() { page.engine.newMatch() })
            }
            MenuItem {
                text: qsTr("How to play")
                onClicked: pageStack.push(Qt.resolvedUrl("GameRules.qml"), { "cardStyle": snapszerEngine.cardStyle })
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("Settings.qml"))
            }
        }

        MultiTable {
            width: flickable.width
            height: flickable.height
        }
    }

    RemorsePopup { id: remorse }
}
