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

// MeeGo Harmattan (Nokia N9) edition of the two-player table. The layout
// follows sailfish/harbour-snapszer.qml; the pulley menu became the tool bar
// menu and remorse popups became query dialogs. Kept in one file with the
// same name so the translation contexts of the Sailfish edition apply.
import QtQuick 1.1
import com.nokia.meego 1.0
import "Style.js" as Style

PageStackWindow {
    id: app
    // Landscape keeps the tool bar but drops the status bar for the table.
    showStatusBar: inPortrait
    showToolBar: true
    initialPage: mainPage

    // One C++ engine instance is injected by main.cpp as a root-context property.
    property QtObject engine: snapszerEngine

    // A LAN connection must survive the opponent's thinking time, so keep the
    // screen on while a LAN game runs (QtMobility, loaded on demand).
    property bool lanActive: engine.networkGame || multiEngine.networkGame
                             || engine.lanBusy || multiEngine.lanBusy
    Loader {
        source: app.lanActive ? Qt.resolvedUrl("KeepDisplayOn.qml") : ""
    }

    Component.onCompleted: theme.inverted = true

    // Debug entry point used by main.cpp (SNAPSZER_OPEN): opens a page or
    // starts a table without touching the screen.
    function openPage(names) {
        var list = names.split(",")
        for (var i = 0; i < list.length; ++i) {
            var name = list[i]
            if (name === "multi3")
                multiEngine.startMatch(3)
            else if (name === "multi4")
                multiEngine.startMatch(4)
            else if (name === "menu")
                mainMenu.open()
            else if (name === "landscape")
                screen.allowedOrientations = Screen.Landscape
            else if (name === "portrait")
                screen.allowedOrientations = Screen.Portrait
            else if (name !== "")
                pageStack.push(Qt.resolvedUrl(name))
        }
    }

    // Three- and four-player matches (local or LAN) open their own table on
    // top of the two-player one.
    Connections {
        target: multiEngine
        onMatchStarted: multiPageTimer.restart()
    }

    Timer {
        id: multiPageTimer
        interval: 60
        onTriggered: {
            if (pageStack.busy) {
                restart()
                return
            }
            if (pageStack.currentPage && pageStack.currentPage.objectName === "multiPage")
                return
            pageStack.pop(null, true)
            pageStack.push(Qt.resolvedUrl("MultiPage.qml"))
        }
    }

    Page {
        id: mainPage

        // Landscape (854x408 under the tool bar): piles move to the left
        // column, the action buttons to the right, and the cards shrink.
        property bool landscape: width > height
        property real cs: landscape ? 0.72 : 1

        property real pendingPlayerStartX: -1
        property real pendingPlayerStartY: -1
        property variant hiddenDealIds: ({})
        property variant hiddenTrickIds: ({})
        property int dealFlightsRemaining: 0
        property int trickFlightsRemaining: 0

        tools: ToolBarLayout {
            ToolIcon {
                iconId: "toolbar-refresh"
                visible: engine.roundOver && !engine.matchOver
                onClicked: mainPage.runAction("nextRound")
            }
            ToolIcon {
                iconId: "toolbar-view-menu"
                onClicked: mainMenu.open()
            }
        }

        Menu {
            id: mainMenu
            visualParent: pageStack
            MenuLayout {
                MenuItem {
                    text: qsTr("Leave LAN game")
                    visible: engine.networkGame
                    onClicked: confirmDialog.ask(qsTr("Leaving the LAN game"), "leaveLan")
                }
                MenuItem {
                    text: qsTr("Play over LAN")
                    visible: !engine.networkGame && !multiEngine.networkGame
                    onClicked: pageStack.push(Qt.resolvedUrl("LanPage.qml"))
                }
                MenuItem {
                    text: multiEngine.active ? qsTr("Back to 3/4-player table")
                                             : qsTr("Continue 3/4-player match")
                    visible: !engine.networkGame && multiEngine.canResume
                    onClicked: mainPage.runAction("multiResume")
                }
                MenuItem {
                    text: qsTr("Play with 3 players")
                    visible: !engine.networkGame && !multiEngine.networkGame
                    onClicked: mainPage.startMulti("multi3")
                }
                MenuItem {
                    text: qsTr("Play with 4 players")
                    visible: !engine.networkGame && !multiEngine.networkGame
                    onClicked: mainPage.startMulti("multi4")
                }
                MenuItem {
                    text: qsTr("New match")
                    onClicked: confirmDialog.ask(qsTr("Starting a new match"), "newMatch")
                }
                MenuItem {
                    text: qsTr("Next round")
                    visible: engine.roundOver && !engine.matchOver
                    onClicked: mainPage.runAction("nextRound")
                }
                MenuItem {
                    text: qsTr("How to play")
                    onClicked: pageStack.push(Qt.resolvedUrl("GameRules.qml"), { "cardStyle": engine.cardStyle })
                }
                MenuItem {
                    text: qsTr("Settings")
                    onClicked: pageStack.push(Qt.resolvedUrl("Settings.qml"))
                }
                MenuItem {
                    text: qsTr("About")
                    onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
                }
            }
        }

        // Replaces the Silica remorse popups: the action runs once confirmed.
        QueryDialog {
            id: confirmDialog
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
                if (action === "closeTalon")
                    engine.closeTalon()
                else
                    mainPage.runAction(action)
            }
        }

        function dur(ms) {
            return Math.max(1, Math.round(ms / Math.max(0.5, engine.animationSpeed)))
        }

        function mapSet(source, key, enabled) {
            var result = ({})
            for (var name in source) {
                if (source[name] && name !== key)
                    result[name] = true
            }
            if (enabled)
                result[key] = true
            return result
        }

        function clearFlights() {
            for (var i = animationLayer.children.length - 1; i >= 0; --i) {
                var child = animationLayer.children[i]
                if (child && child.isFlyingCard)
                    child.destroy()
            }
            hiddenDealIds = ({})
            hiddenTrickIds = ({})
            dealFlightsRemaining = 0
            trickFlightsRemaining = 0
        }

        function indexOfCard(cards, id) {
            for (var i = 0; i < cards.length; ++i) {
                if (cards[i].id === id)
                    return i
            }
            return -1
        }

        function handCardWidth(count) {
            var maxWidth = Style.itemSizeLarge * 1.14 * cs
            var available = handArea.width - 2 * Style.paddingSmall
            if (count <= 1)
                return Math.min(maxWidth, available)
            return Math.min(maxWidth, available / (1 + 0.52 * (count - 1)))
        }

        function handCardX(index, count, width) {
            var total = width + Math.max(0, count - 1) * width * 0.52
            return (handArea.width - total) / 2 + index * width * 0.52
        }

        function playerCardCenter(index, count) {
            var width = handCardWidth(count)
            return handArea.mapToItem(animationLayer,
                                      handCardX(index, count, width) + width / 2,
                                      width * 1.4 / 2)
        }

        function aiCardWidth(count) {
            var maxWidth = Style.itemSizeMedium * 1.02 * cs
            var available = aiHandArea.width - 2 * Style.paddingSmall
            if (count <= 1)
                return Math.min(maxWidth, available)
            return Math.min(maxWidth, available / (1 + 0.48 * (count - 1)))
        }

        function aiCardX(index, count, width) {
            var total = width + Math.max(0, count - 1) * width * 0.48
            return (aiHandArea.width - total) / 2 + index * width * 0.48
        }

        function aiCardCenter(index, count) {
            var width = aiCardWidth(count)
            return aiHandArea.mapToItem(animationLayer,
                                        aiCardX(index, count, width) + width / 2,
                                        width * 1.4 / 2)
        }

        function trickCenter(index) {
            var w = Style.itemSizeLarge * 1.08 * cs
            var dx = w * 0.28
            return trickArea.mapToItem(animationLayer,
                                       trickArea.width / 2 + (index === 0 ? -dx : dx),
                                       trickArea.height / 2 + (index === 0 ? -Style.paddingSmall : Style.paddingSmall))
        }

        function talonCenter() {
            return talonDeck.mapToItem(animationLayer, talonDeck.width / 2, talonDeck.height / 2)
        }

        function wonCenter(player) {
            var item = player === 0 ? playerWonPile : cpuWonPile
            return item.mapToItem(animationLayer, item.width / 2, item.height / 2)
        }

        function spawnFlight(id, faceUp, fromPoint, toPoint, delay, role, rotation, callback) {
            var object = flyingCardComponent.createObject(animationLayer, {
                "cardId": id,
                "cardStyle": engine.cardStyle,
                "faceUp": faceUp,
                "fromX": fromPoint.x,
                "fromY": fromPoint.y,
                "toX": toPoint.x,
                "toY": toPoint.y,
                "startDelay": delay,
                "flightDuration": dur(role === "deal" ? 300 : 330),
                "fromRotation": role === "trick" ? rotation : 0,
                "toRotation": role === "capture" ? rotation : 0
            })
            if (!object)
                return null
            object.finished.connect(function() {
                callback(object)
            })
            return object
        }

        function beginPlayerPlay(index, declareMarriage) {
            engine.playCard(index, declareMarriage)
        }

        // Menu and dialog actions; the same names as the pulley actions of
        // the Sailfish edition.
        function runAction(action) {
            marriagePanel.visible = false
            // A LAN guest only sends a request here; its running animations
            // stay valid until the host's answer arrives.
            if (!engine.lanGuest)
                clearFlights()
            if (action === "newMatch")
                engine.newMatch()
            else if (action === "nextRound")
                engine.nextRound()
            else if (action === "leaveLan")
                engine.cancelLan()
            else if (action === "multi3")
                multiEngine.startMatch(3)
            else if (action === "multi4")
                multiEngine.startMatch(4)
            else if (action === "multiResume")
                multiEngine.resume()
        }

        // A running 3/4-player match is only replaced after confirmation.
        function startMulti(action) {
            // canResume without an active match means an unfinished saved one.
            if ((multiEngine.active && !multiEngine.matchOver) || (!multiEngine.active && multiEngine.canResume))
                confirmDialog.ask(qsTr("Replacing the running 3/4-player match"), action)
            else
                runAction(action)
        }

        function askMarriage(index, value, sx, sy) {
            marriagePanel.handIndex = index
            marriagePanel.marriageValue = value
            marriagePanel.startX = sx
            marriagePanel.startY = sy
            marriagePanel.visible = true
        }

        function dealFlightFinished(object) {
            hiddenDealIds = mapSet(hiddenDealIds, object.cardId, false)
            object.destroy()
            dealFlightsRemaining--
            if (dealFlightsRemaining <= 0) {
                hiddenDealIds = ({})
                engine.completeDealAnimation()
            }
        }

        function trickFlightFinished(object) {
            object.destroy()
            trickFlightsRemaining--
            if (trickFlightsRemaining <= 0) {
                hiddenTrickIds = ({})
                engine.completeTrickAnimation()
            }
        }

        Rectangle {
            anchors.fill: parent
            color: Style.tableColor
        }

        Item {
            id: table
            anchors.fill: parent

            Text {
                id: matchLabel
                anchors.top: parent.top
                anchors.topMargin: Style.paddingSmall
                anchors.horizontalCenter: parent.horizontalCenter
                width: parent.width - 2 * Style.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Match  %1  %2 : %3  %4")
                      .arg(engine.playerName).arg(engine.playerGamePoints)
                      .arg(engine.cpuGamePoints).arg(engine.opponentDisplayName)
                font.pixelSize: Style.fontSizeExtraSmall
                color: Style.secondaryHighlightColor
                elide: Text.ElideRight
                z: 20
            }

            Item {
                id: aiHandArea
                anchors.top: matchLabel.bottom
                anchors.topMargin: Style.paddingSmall
                anchors.horizontalCenter: parent.horizontalCenter
                width: mainPage.landscape ? parent.width * 0.5 : parent.width - 2 * Style.paddingLarge
                height: Style.itemSizeMedium * 1.42 * mainPage.cs

                Repeater {
                    model: engine.cpuHand
                    Item {
                        property real cw: mainPage.aiCardWidth(engine.cpuHand.length)
                        width: cw
                        height: cw * 1.4
                        x: mainPage.aiCardX(index, engine.cpuHand.length, cw)
                        y: 0
                        z: index
                        opacity: mainPage.hiddenDealIds[modelData.id] ? 0 : 1
                        Card {
                            anchors.fill: parent
                            cardId: modelData.id
                            cardStyle: engine.cardStyle
                            faceUp: false
                        }
                    }
                }
            }

            Text {
                id: cpuInfo
                anchors.top: aiHandArea.bottom
                anchors.topMargin: -Style.paddingSmall
                anchors.horizontalCenter: parent.horizontalCenter
                text: engine.opponentDisplayName + "  •  " + qsTr("%1 points").arg(engine.cpuPoints)
                font.pixelSize: Style.fontSizeExtraSmall
                color: Style.secondaryColor
            }

            Item {
                id: cpuWonPile
                anchors.right: parent.right
                anchors.rightMargin: Style.paddingSmall
                anchors.leftMargin: Style.paddingLarge
                anchors.top: cpuInfo.bottom
                anchors.topMargin: mainPage.landscape ? Style.paddingLarge : Style.paddingSmall
                width: Style.itemSizeMedium * 0.72 * mainPage.cs
                height: width * 1.4
                Card {
                    anchors.fill: parent
                    visible: engine.cpuWonCards.length > 0
                    cardStyle: engine.cardStyle
                    cardId: engine.cpuWonCards.length ? engine.cpuWonCards[0].id : "1_10"
                    faceUp: false
                    rotation: 5
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.bottom
                    text: engine.opponentDisplayName + " " + Math.floor(engine.cpuWonCards.length / 2)
                    font.pixelSize: Style.fontSizeExtraSmall
                    color: Style.secondaryColor
                }
            }

            Item {
                id: talonArea
                anchors.left: parent.left
                anchors.leftMargin: Style.paddingLarge
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: mainPage.landscape ? 0 : -Style.itemSizeSmall * 0.45
                width: Style.itemSizeLarge * 1.22 * mainPage.cs
                height: Style.itemSizeLarge * 1.55 * mainPage.cs

                Card {
                    id: trumpVisual
                    width: Style.itemSizeMedium * 0.92 * mainPage.cs
                    height: width * 1.4
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: Style.paddingMedium
                    visible: engine.hasTrumpCard
                    cardId: engine.hasTrumpCard && engine.trumpCard.id ? engine.trumpCard.id : "0_11"
                    cardStyle: engine.cardStyle
                    faceUp: true
                    rotation: 90
                }

                Card {
                    id: talonDeck
                    width: Style.itemSizeMedium * 0.98 * mainPage.cs
                    height: width * 1.4
                    anchors.centerIn: parent
                    anchors.horizontalCenterOffset: -Style.paddingSmall
                    visible: engine.faceDownStockSize > 0
                    cardId: "0_10"
                    cardStyle: engine.cardStyle
                    faceUp: false
                    rotation: engine.talonClosed ? 90 : 0
                }

                Text {
                    anchors.top: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: engine.talonClosed ? qsTr("Closed")
                          : engine.talonSize > 0 ? qsTr("Talon %1").arg(engine.talonSize)
                          : qsTr("Strict play")
                    font.pixelSize: Style.fontSizeExtraSmall
                    color: engine.talonClosed ? Style.highlightColor : Style.secondaryColor
                }
            }

            Item {
                id: trickArea
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -Style.itemSizeSmall * 0.28
                width: parent.width * 0.52
                height: Style.itemSizeLarge * 1.60 * mainPage.cs

                Repeater {
                    model: engine.trickCards
                    Card {
                        width: Style.itemSizeLarge * 1.08 * mainPage.cs
                        height: width * 1.4
                        cardId: modelData.id
                        cardStyle: engine.cardStyle
                        faceUp: true
                        x: trickArea.width / 2 - width / 2 + (index === 0 ? -width * 0.28 : width * 0.28)
                        y: trickArea.height / 2 - height / 2 + (index === 0 ? -Style.paddingSmall : Style.paddingSmall)
                        rotation: index === 0 ? -6 : 7
                        opacity: mainPage.hiddenTrickIds[modelData.id] ? 0 : 1
                        z: index
                    }
                }
            }

            Text {
                id: statusLabel
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: trickArea.bottom
                anchors.topMargin: Style.paddingSmall
                width: mainPage.landscape ? parent.width * 0.5 : parent.width - 2 * Style.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                text: engine.status
                font.pixelSize: Style.fontSizeSmall
                color: Style.primaryColor
                wrapMode: Text.WordWrap
            }

            Text {
                id: playerInfo
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: statusLabel.bottom
                anchors.topMargin: Style.paddingSmall
                text: engine.playerName + "  •  " + qsTr("%1 points").arg(engine.playerPoints)
                font.pixelSize: Style.fontSizeExtraSmall
                color: Style.secondaryColor
            }

            Flow {
                id: actionRow
                property real buttonHeight: Style.itemSizeSmall * 0.86 * mainPage.cs
                flow: mainPage.landscape ? Flow.TopToBottom : Flow.LeftToRight
                // Portrait: a row left of the player's pile. Landscape: a
                // column at the right edge. Width is computed, not anchored,
                // so the same item serves both.
                anchors.left: parent.left
                anchors.leftMargin: Style.paddingSmall
                anchors.rightMargin: Style.paddingLarge
                anchors.bottom: handArea.top
                anchors.bottomMargin: Style.paddingSmall
                width: mainPage.landscape ? Style.itemSizeLarge * 1.4
                                          : playerWonPile.x - Style.paddingMedium - Style.paddingSmall
                height: mainPage.landscape ? 3 * buttonHeight + 2 * spacing : buttonHeight
                spacing: Style.paddingSmall
                visible: !engine.roundOver

                Repeater {
                    model: 3
                    Rectangle {
                        id: actionBox
                        property bool canAct: index === 0 ? engine.canExchangeTrump
                                            : index === 1 ? engine.canCloseTalon : engine.canClaim66
                        width: mainPage.landscape ? actionRow.width : (actionRow.width - 2 * actionRow.spacing) / 3
                        height: actionRow.buttonHeight
                        radius: Style.paddingSmall
                        color: actionMouse.pressed && canAct ? "#40ffffff" : "#22ffffff"
                        opacity: canAct ? 1.0 : 0.28
                        Text {
                            anchors.centerIn: parent
                            text: index === 0 ? qsTr("Exchange") : index === 1 ? qsTr("Close talon") : qsTr("66!")
                            font.pixelSize: index === 2 ? Style.fontSizeSmall : Style.fontSizeExtraSmall
                            font.bold: index === 2
                            color: actionBox.canAct ? Style.highlightColor : Style.secondaryColor
                        }
                        MouseArea {
                            id: actionMouse
                            anchors.fill: parent
                            enabled: actionBox.canAct
                            onClicked: {
                                if (index === 0)
                                    engine.exchangeTrump()
                                else if (index === 1)
                                    confirmDialog.ask(qsTr("Closing the talon"), "closeTalon")
                                else
                                    engine.claim66()
                            }
                        }
                    }
                }
            }

            Item {
                id: handArea
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: mainPage.landscape ? parent.width * 0.24 : 0
                anchors.rightMargin: mainPage.landscape ? parent.width * 0.24 : 0
                anchors.bottom: parent.bottom
                anchors.bottomMargin: Style.paddingSmall
                height: Math.min(parent.height * 0.25, Style.itemSizeLarge * 1.65 * mainPage.cs)

                Repeater {
                    model: engine.playerHand
                    Item {
                        property real cw: mainPage.handCardWidth(engine.playerHand.length)
                        width: cw
                        height: cw * 1.4
                        x: mainPage.handCardX(index, engine.playerHand.length, cw)
                        y: 0
                        z: playerMouse.pressed ? 100 : index
                        opacity: mainPage.hiddenDealIds[modelData.id] ? 0
                                 : (engine.playerInputEnabled && engine.isPlayerCardPlayable(index)) ? 1.0 : 0.52

                        Card {
                            id: handCard
                            anchors.fill: parent
                            cardId: modelData.id
                            cardStyle: engine.cardStyle
                            faceUp: true
                            pressed: playerMouse.pressed
                        }

                        MouseArea {
                            id: playerMouse
                            anchors.fill: parent
                            enabled: engine.playerInputEnabled
                                     && engine.isPlayerCardPlayable(index)
                                     && !marriagePanel.visible
                            onClicked: {
                                var start = playerMouse.mapToItem(animationLayer, width / 2, height / 2)
                                mainPage.pendingPlayerStartX = start.x
                                mainPage.pendingPlayerStartY = start.y
                                var value = engine.marriagePointsForCard(index)
                                if (value > 0)
                                    mainPage.askMarriage(index, value, start.x, start.y)
                                else
                                    mainPage.beginPlayerPlay(index, false)
                            }
                        }
                    }
                }
            }

            Item {
                id: playerWonPile
                anchors.right: handArea.right
                anchors.rightMargin: Style.paddingSmall
                anchors.leftMargin: Style.paddingLarge
                anchors.bottom: handArea.top
                anchors.bottomMargin: mainPage.landscape ? Style.paddingLarge : Style.paddingMedium
                width: Style.itemSizeMedium * 0.82 * mainPage.cs
                height: width * 1.4
                z: 2

                Card {
                    anchors.fill: parent
                    visible: engine.playerWonCards.length > 0
                    cardStyle: engine.cardStyle
                    cardId: engine.playerWonCards.length ? engine.playerWonCards[0].id : "0_10"
                    faceUp: false
                    rotation: -5
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.top
                    anchors.bottomMargin: Style.paddingSmall
                    text: qsTr("You %1").arg(Math.floor(engine.playerWonCards.length / 2))
                    font.pixelSize: Style.fontSizeExtraSmall
                    color: Style.secondaryColor
                }
            }

            Rectangle {
                id: marriagePanel
                property int handIndex: -1
                property int marriageValue: 0
                property real startX: -1
                property real startY: -1
                visible: false
                z: 500
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: handArea.top
                anchors.bottomMargin: Style.paddingSmall
                width: mainPage.landscape ? parent.width * 0.5 : parent.width - 2 * Style.horizontalPageMargin
                height: Style.itemSizeLarge * 1.08
                radius: Style.paddingSmall
                color: "#ee202020"
                border.color: Style.highlightColor

                Column {
                    anchors.fill: parent
                    anchors.margins: Style.paddingSmall
                    spacing: Style.paddingSmall
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Declare %1?").arg(marriagePanel.marriageValue)
                        font.pixelSize: Style.fontSizeSmall
                        color: Style.highlightColor
                    }
                    Row {
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: Style.paddingMedium
                        TableButton {
                            text: qsTr("Declare")
                            onClicked: {
                                mainPage.pendingPlayerStartX = marriagePanel.startX
                                mainPage.pendingPlayerStartY = marriagePanel.startY
                                var idx = marriagePanel.handIndex
                                marriagePanel.visible = false
                                engine.playCard(idx, true)
                            }
                        }
                        TableButton {
                            text: qsTr("Play only")
                            onClicked: {
                                mainPage.pendingPlayerStartX = marriagePanel.startX
                                mainPage.pendingPlayerStartY = marriagePanel.startY
                                var idx = marriagePanel.handIndex
                                marriagePanel.visible = false
                                engine.playCard(idx, false)
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: resultPanel
                visible: engine.roundOver
                z: 450
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: mainPage.landscape ? parent.width * 0.6 : parent.width - 2 * Style.horizontalPageMargin
                height: resultColumn.height + 2 * Style.paddingLarge
                radius: Style.paddingSmall
                color: Style.panelColor
                border.color: Style.highlightColor

                Column {
                    id: resultColumn
                    anchors.centerIn: parent
                    width: parent.width - 2 * Style.paddingLarge
                    spacing: Style.paddingMedium
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: engine.matchOver ? engine.status : qsTr("Round over")
                        font.pixelSize: Style.fontSizeLarge
                        color: Style.highlightColor
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: engine.roundResult
                        font.pixelSize: Style.fontSizeSmall
                        color: Style.primaryColor
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Match: %1 : %2").arg(engine.playerGamePoints).arg(engine.cpuGamePoints)
                        font.pixelSize: Style.fontSizeMedium
                        color: Style.secondaryColor
                    }
                    TableButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: engine.matchOver ? qsTr("New match") : qsTr("Next round")
                        onClicked: {
                            if (!engine.lanGuest)
                                mainPage.clearFlights()
                            if (engine.matchOver)
                                engine.newMatch()
                            else
                                engine.nextRound()
                        }
                    }
                }
            }

            Rectangle {
                id: noticePanel
                property alias text: noticeLabel.text
                visible: false
                z: 600
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: matchLabel.bottom
                anchors.topMargin: Style.paddingLarge
                width: mainPage.landscape ? parent.width * 0.6 : parent.width - 2 * Style.horizontalPageMargin
                height: noticeLabel.height + 2 * Style.paddingMedium
                radius: Style.paddingSmall
                color: Style.panelColor
                border.color: Style.highlightColor

                Text {
                    id: noticeLabel
                    anchors.centerIn: parent
                    width: parent.width - 2 * Style.paddingMedium
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    font.pixelSize: Style.fontSizeSmall
                    color: Style.highlightColor
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: noticePanel.visible = false
                }
                Timer {
                    id: noticeTimer
                    interval: 5000
                    onTriggered: noticePanel.visible = false
                }
            }
        }

        Item { id: animationLayer; anchors.fill: parent; z: 1000 }

        // Landscape: the piles form a left column, the action buttons a
        // right column. States, because Qt 4 cannot reset anchors from a
        // binding.
        states: State {
            name: "landscape"
            when: mainPage.landscape
            AnchorChanges {
                target: cpuWonPile
                anchors.right: undefined
                anchors.left: table.left
                anchors.top: matchLabel.bottom
            }
            AnchorChanges {
                target: playerWonPile
                anchors.right: undefined
                anchors.left: table.left
                anchors.bottom: table.bottom
            }
            AnchorChanges {
                target: actionRow
                anchors.left: undefined
                anchors.bottom: undefined
                anchors.right: table.right
                anchors.verticalCenter: table.verticalCenter
            }
        }

        Component {
            id: flyingCardComponent
            FlyingCard { }
        }

        Connections {
            target: multiEngine
            onNetworkNotice: {
                noticePanel.text = text
                noticePanel.visible = true
                noticeTimer.restart()
            }
        }

        Connections {
            target: engine

            onCardAnimationRequested: {
                if (!engine.animationsEnabled)
                    return
                var fromPoint
                if (playedBy === 0 && mainPage.pendingPlayerStartX >= 0) {
                    fromPoint = Qt.point(mainPage.pendingPlayerStartX, mainPage.pendingPlayerStartY)
                } else if (playedBy === 0) {
                    fromPoint = mainPage.playerCardCenter(oldHandIndex, engine.playerHand.length + 1)
                } else {
                    fromPoint = mainPage.aiCardCenter(oldHandIndex, engine.cpuHand.length + 1)
                }
                var targetIndex = engine.trickCards.length - 1
                var toPoint = mainPage.trickCenter(targetIndex)
                mainPage.hiddenTrickIds = mainPage.mapSet(mainPage.hiddenTrickIds, cardId, true)
                var object = mainPage.spawnFlight(cardId, true, fromPoint, toPoint, 0,
                                                  "trick", playedBy === 0 ? -4 : 4,
                                                  function(obj) {
                    mainPage.hiddenTrickIds = mainPage.mapSet(mainPage.hiddenTrickIds, obj.cardId, false)
                    obj.destroy()
                    mainPage.pendingPlayerStartX = -1
                    mainPage.pendingPlayerStartY = -1
                    engine.completeCardAnimation()
                })
                if (!object) {
                    mainPage.hiddenTrickIds = ({})
                    engine.completeCardAnimation()
                }
            }

            onTrickAnimationRequested: {
                if (!engine.animationsEnabled)
                    return
                mainPage.trickFlightsRemaining = engine.trickCards.length
                if (mainPage.trickFlightsRemaining === 0) {
                    engine.completeTrickAnimation()
                    return
                }
                for (var i = 0; i < engine.trickCards.length; ++i) {
                    var card = engine.trickCards[i]
                    mainPage.hiddenTrickIds = mainPage.mapSet(mainPage.hiddenTrickIds, card.id, true)
                    var start = mainPage.trickCenter(i)
                    var end = mainPage.wonCenter(winnerPlayer)
                    var flight = mainPage.spawnFlight(card.id, true, start, end,
                                                      i * mainPage.dur(70), "capture",
                                                      i === 0 ? -9 : 8,
                                                      mainPage.trickFlightFinished)
                    if (!flight) {
                        mainPage.trickFlightsRemaining--
                    }
                }
                if (mainPage.trickFlightsRemaining <= 0) {
                    mainPage.hiddenTrickIds = ({})
                    engine.completeTrickAnimation()
                }
            }

            onResetVisuals: {
                mainPage.clearFlights()
                marriagePanel.visible = false
                mainPage.pendingPlayerStartX = -1
                mainPage.pendingPlayerStartY = -1
            }

            onNetworkNotice: {
                noticePanel.text = text
                noticePanel.visible = true
                noticeTimer.restart()
            }

            onDealAnimationRequested: {
                mainPage.clearFlights()
                if (!engine.animationsEnabled)
                    return
                mainPage.dealFlightsRemaining = cards.length
                if (cards.length === 0) {
                    engine.completeDealAnimation()
                    return
                }
                var start = mainPage.talonCenter()
                for (var i = 0; i < cards.length; ++i) {
                    var value = cards[i]
                    mainPage.hiddenDealIds = mainPage.mapSet(mainPage.hiddenDealIds, value.id, true)
                }
                for (var j = 0; j < cards.length; ++j) {
                    var draw = cards[j]
                    var targetCards = draw.player === 0 ? engine.playerHand : engine.cpuHand
                    var finalIndex = mainPage.indexOfCard(targetCards, draw.id)
                    var end = draw.player === 0
                            ? mainPage.playerCardCenter(finalIndex, targetCards.length)
                            : mainPage.aiCardCenter(finalIndex, targetCards.length)
                    var flight = mainPage.spawnFlight(draw.id, draw.player === 0,
                                                      start, end,
                                                      j * mainPage.dur(75), "deal", 0,
                                                      mainPage.dealFlightFinished)
                    if (!flight) {
                        mainPage.hiddenDealIds = mainPage.mapSet(mainPage.hiddenDealIds, draw.id, false)
                        mainPage.dealFlightsRemaining--
                    }
                }
                if (mainPage.dealFlightsRemaining <= 0) {
                    mainPage.hiddenDealIds = ({})
                    engine.completeDealAnimation()
                }
            }
        }

        onStatusChanged: engine.paused = status !== PageStatus.Active
        Component.onCompleted: {
            engine.paused = status !== PageStatus.Active
            engine.start()
        }
    }
}
