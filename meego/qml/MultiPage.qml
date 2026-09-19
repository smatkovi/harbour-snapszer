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
    objectName: "multiPage"

    property QtObject engine: multiEngine

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

    // A LAN table is left with "Leave LAN game", so the back icon cannot
    // strand the seat of a player who is still connected.
    tools: ToolBarLayout {
        ToolIcon {
            iconId: "toolbar-back"
            visible: !page.engine.networkGame
            onClicked: pageStack.pop()
        }
        ToolIcon {
            iconId: "toolbar-view-menu"
            onClicked: menu.open()
        }
    }

    Menu {
        id: menu
        visualParent: pageStack
        MenuLayout {
            MenuItem {
                text: qsTr("Leave LAN game")
                visible: page.engine.networkGame
                onClicked: confirm.ask(qsTr("Leaving the LAN game"), "leaveLan")
            }
            MenuItem {
                text: qsTr("New match")
                visible: !page.engine.lanGuest
                onClicked: confirm.ask(qsTr("Starting a new match"), "newMatch")
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
    }

    QueryDialog {
        id: confirm
        property string action: ""
        acceptButtonText: qsTr("Yes")
        rejectButtonText: qsTr("No")
        function ask(title, what) {
            titleText = title
            message = qsTr("Are you sure?")
            action = what
            open()
        }
        onAccepted: {
            if (action === "leaveLan")
                page.engine.cancelLan()
            else if (action === "newMatch")
                page.engine.newMatch()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Style.tableColor
    }

    MultiTable {
        anchors.fill: parent
    }
}
