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
import Nemo.KeepAlive 1.2
ApplicationWindow {
    id: app

    // One C++ engine instance is injected by main.cpp as a root-context property.
    // Keeping a QML alias preserves the existing table bindings while allowing
    // dynamically-created pages to access the same instance directly.
    property var engine: snapszerEngine

    // A LAN connection must survive the opponent's thinking time, so keep the
    // screen on while playing and the device awake while in the background.
    readonly property bool lanActive: engine.networkGame || multiEngine.networkGame
                                      || engine.lanBusy || multiEngine.lanBusy
    DisplayBlanking { preventBlanking: app.lanActive && Qt.application.active }
    KeepAlive { enabled: app.lanActive }

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
            pageStack.pop(null, PageStackAction.Immediate)
            pageStack.push(Qt.resolvedUrl("MultiPage.qml"))
        }
    }

    cover: Component {
        CoverPage {
            playerName: engine.playerName
            opponentName: engine.opponentDisplayName
            playerGamePoints: engine.playerGamePoints
            opponentGamePoints: engine.cpuGamePoints
            playerPoints: engine.playerPoints
            opponentPoints: engine.cpuPoints
            cardStyle: engine.cardStyle
            trumpCard: engine.trumpCard
            hasTrumpCard: engine.hasTrumpCard
            talonClosed: engine.talonClosed
        }
    }

    initialPage: Component {
        Page {
            id: mainPage
            allowedOrientations: Orientation.Portrait

            property real pendingPlayerStartX: -1
            property real pendingPlayerStartY: -1
            property var hiddenDealIds: ({})
            property var hiddenTrickIds: ({})
            property int dealFlightsRemaining: 0
            property int trickFlightsRemaining: 0
            property string pendingPulleyAction: ""

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
                var maxWidth = Theme.itemSizeLarge * 1.14
                var available = handArea.width - 2 * Theme.paddingSmall
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
                var maxWidth = Theme.itemSizeMedium * 1.02
                var available = aiHandArea.width - 2 * Theme.paddingSmall
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
                var w = Theme.itemSizeLarge * 1.08
                var dx = w * 0.28
                return trickArea.mapToItem(animationLayer,
                                           trickArea.width / 2 + (index === 0 ? -dx : dx),
                                           trickArea.height / 2 + (index === 0 ? -Theme.paddingSmall : Theme.paddingSmall))
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

            function requestPulleyAction(action) {
                pendingPulleyAction = action
                pulleyActionTimer.restart()
            }

            function tryPulleyAction() {
                if (pendingPulleyAction === "")
                    return
                if (pulleyMenu.active || flickable.moving || flickable.dragging || flickable.flicking) {
                    pulleyActionTimer.restart()
                    return
                }
                var action = pendingPulleyAction
                pendingPulleyAction = ""
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

            // A running 3/4-player match is only replaced after a countdown.
            function startMulti(action) {
                if (multiEngine.active && !multiEngine.matchOver)
                    newMatchRemorse.execute(qsTr("Replacing the running 3/4-player match"), function() {
                        mainPage.requestPulleyAction(action)
                    })
                else
                    requestPulleyAction(action)
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
                color: "#0b5d36"
            }

            SilicaFlickable {
                id: flickable
                anchors.fill: parent
                contentHeight: height

                onMovementEnded: mainPage.tryPulleyAction()

                PullDownMenu {
                    id: pulleyMenu
                    onActiveChanged: if (!active) mainPage.tryPulleyAction()

                    MenuItem {
                        text: qsTr("Leave LAN game")
                        visible: engine.networkGame
                        onClicked: newMatchRemorse.execute(qsTr("Leaving the LAN game"), function() {
                            mainPage.requestPulleyAction("leaveLan")
                        })
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
                        onClicked: mainPage.requestPulleyAction("multiResume")
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
                        onClicked: newMatchRemorse.execute(qsTr("Starting a new match"), function() {
                            mainPage.requestPulleyAction("newMatch")
                        })
                    }
                    MenuItem {
                        text: qsTr("Next round")
                        visible: engine.roundOver && !engine.matchOver
                        onClicked: mainPage.requestPulleyAction("nextRound")
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

                Item {
                    id: table
                    width: flickable.width
                    height: flickable.height

                    Label {
                        id: matchLabel
                        anchors.top: parent.top
                        anchors.topMargin: Theme.paddingSmall
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Match  %1  %2 : %3  %4")
                              .arg(engine.playerName).arg(engine.playerGamePoints)
                              .arg(engine.cpuGamePoints).arg(engine.opponentDisplayName)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryHighlightColor
                        truncationMode: TruncationMode.Fade
                        z: 20
                    }

                    Item {
                        id: aiHandArea
                        anchors.top: matchLabel.bottom
                        anchors.topMargin: Theme.paddingSmall
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width - 2 * Theme.paddingLarge
                        height: Theme.itemSizeMedium * 1.42

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

                    Label {
                        id: cpuInfo
                        anchors.top: aiHandArea.bottom
                        anchors.topMargin: -Theme.paddingSmall
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: engine.opponentDisplayName + "  •  " + qsTr("%1 points").arg(engine.cpuPoints)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }

                    Item {
                        id: cpuWonPile
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.paddingSmall
                        anchors.top: cpuInfo.bottom
                        anchors.topMargin: Theme.paddingSmall
                        width: Theme.itemSizeMedium * 0.72
                        height: width * 1.4
                        Card {
                            anchors.fill: parent
                            visible: engine.cpuWonCards.length > 0
                            cardStyle: engine.cardStyle
                            cardId: engine.cpuWonCards.length ? engine.cpuWonCards[0].id : "1_10"
                            faceUp: false
                            rotation: 5
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.bottom
                            text: engine.opponentDisplayName + " " + Math.floor(engine.cpuWonCards.length / 2)
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                        }
                    }

                    Item {
                        id: talonArea
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.paddingLarge
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -Theme.itemSizeSmall * 0.45
                        width: Theme.itemSizeLarge * 1.22
                        height: Theme.itemSizeLarge * 1.55

                        Card {
                            id: trumpVisual
                            width: Theme.itemSizeMedium * 0.92
                            height: width * 1.4
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: Theme.paddingMedium
                            visible: engine.hasTrumpCard
                            cardId: engine.hasTrumpCard && engine.trumpCard.id ? engine.trumpCard.id : "0_11"
                            cardStyle: engine.cardStyle
                            faceUp: true
                            rotation: 90
                        }

                        Card {
                            id: talonDeck
                            width: Theme.itemSizeMedium * 0.98
                            height: width * 1.4
                            anchors.centerIn: parent
                            anchors.horizontalCenterOffset: -Theme.paddingSmall
                            visible: engine.faceDownStockSize > 0
                            cardId: "0_10"
                            cardStyle: engine.cardStyle
                            faceUp: false
                            rotation: engine.talonClosed ? 90 : 0
                        }

                        Label {
                            anchors.top: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: engine.talonClosed ? qsTr("Closed")
                                  : engine.talonSize > 0 ? qsTr("Talon %1").arg(engine.talonSize)
                                  : qsTr("Strict play")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: engine.talonClosed ? Theme.highlightColor : Theme.secondaryColor
                        }
                    }

                    Item {
                        id: trickArea
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: -Theme.itemSizeSmall * 0.28
                        width: parent.width * 0.52
                        height: Theme.itemSizeLarge * 1.60

                        Repeater {
                            model: engine.trickCards
                            Card {
                                width: Theme.itemSizeLarge * 1.08
                                height: width * 1.4
                                cardId: modelData.id
                                cardStyle: engine.cardStyle
                                faceUp: true
                                x: parent.width / 2 - width / 2 + (index === 0 ? -width * 0.28 : width * 0.28)
                                y: parent.height / 2 - height / 2 + (index === 0 ? -Theme.paddingSmall : Theme.paddingSmall)
                                rotation: index === 0 ? -6 : 7
                                opacity: mainPage.hiddenTrickIds[modelData.id] ? 0 : 1
                                z: index
                            }
                        }
                    }

                    Label {
                        id: statusLabel
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: trickArea.bottom
                        anchors.topMargin: Theme.paddingSmall
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        horizontalAlignment: Text.AlignHCenter
                        text: engine.status
                        font.pixelSize: Theme.fontSizeSmall
                        color: Theme.primaryColor
                        wrapMode: Text.WordWrap
                    }

                    Label {
                        id: playerInfo
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: statusLabel.bottom
                        anchors.topMargin: Theme.paddingSmall
                        text: engine.playerName + "  •  " + qsTr("%1 points").arg(engine.playerPoints)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }

                    Row {
                        id: actionRow
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.paddingSmall
                        anchors.right: playerWonPile.left
                        anchors.rightMargin: Theme.paddingMedium
                        anchors.bottom: handArea.top
                        anchors.bottomMargin: Theme.paddingSmall
                        height: Theme.itemSizeSmall * 0.86
                        spacing: Theme.paddingSmall
                        visible: !engine.roundOver

                        BackgroundItem {
                            width: (actionRow.width - 2 * actionRow.spacing) / 3
                            height: parent.height
                            enabled: engine.canExchangeTrump
                            opacity: enabled ? 1.0 : 0.28
                            onClicked: engine.exchangeTrump()
                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Exchange")
                                font.pixelSize: Theme.fontSizeExtraSmall
                                color: parent.enabled ? Theme.highlightColor : Theme.secondaryColor
                            }
                        }
                        BackgroundItem {
                            width: (actionRow.width - 2 * actionRow.spacing) / 3
                            height: parent.height
                            enabled: engine.canCloseTalon
                            opacity: enabled ? 1.0 : 0.28
                            onClicked: closeRemorse.execute(qsTr("Closing the talon"), function() { engine.closeTalon() })
                            Label {
                                anchors.centerIn: parent
                                text: qsTr("Close talon")
                                font.pixelSize: Theme.fontSizeExtraSmall
                                color: parent.enabled ? Theme.highlightColor : Theme.secondaryColor
                            }
                        }
                        BackgroundItem {
                            width: (actionRow.width - 2 * actionRow.spacing) / 3
                            height: parent.height
                            enabled: engine.canClaim66
                            opacity: enabled ? 1.0 : 0.28
                            onClicked: engine.claim66()
                            Label {
                                anchors.centerIn: parent
                                text: qsTr("66!")
                                font.pixelSize: Theme.fontSizeSmall
                                font.bold: true
                                color: parent.enabled ? Theme.highlightColor : Theme.secondaryColor
                            }
                        }
                    }

                    Item {
                        id: handArea
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: Theme.paddingSmall
                        height: Math.min(parent.height * 0.25, Theme.itemSizeLarge * 1.65)

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
                                        var scene = playerMouse.mapToItem(null, width / 2, height / 2)
                                        var start = animationLayer.mapFromItem(null, scene.x, scene.y)
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
                        anchors.rightMargin: Theme.paddingSmall
                        anchors.bottom: handArea.top
                        anchors.bottomMargin: Theme.paddingMedium
                        width: Theme.itemSizeMedium * 0.82
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

                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.bottom: parent.top
                            anchors.bottomMargin: Theme.paddingSmall
                            text: qsTr("You %1").arg(Math.floor(engine.playerWonCards.length / 2))
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
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
                        anchors.bottomMargin: Theme.paddingSmall
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        height: Theme.itemSizeLarge * 1.08
                        radius: Theme.paddingSmall
                        color: "#ee202020"
                        border.color: Theme.highlightColor

                        Column {
                            anchors.fill: parent
                            anchors.margins: Theme.paddingSmall
                            spacing: Theme.paddingSmall
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Declare %1?").arg(marriagePanel.marriageValue)
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.highlightColor
                            }
                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: Theme.paddingMedium
                                Button {
                                    text: qsTr("Declare")
                                    onClicked: {
                                        mainPage.pendingPlayerStartX = marriagePanel.startX
                                        mainPage.pendingPlayerStartY = marriagePanel.startY
                                        var idx = marriagePanel.handIndex
                                        marriagePanel.visible = false
                                        engine.playCard(idx, true)
                                    }
                                }
                                Button {
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
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        height: resultColumn.height + 2 * Theme.paddingLarge
                        radius: Theme.paddingSmall
                        color: "#f0202020"
                        border.color: Theme.highlightColor

                        Column {
                            id: resultColumn
                            anchors.centerIn: parent
                            width: parent.width - 2 * Theme.paddingLarge
                            spacing: Theme.paddingMedium
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: engine.matchOver ? engine.status : qsTr("Round over")
                                font.pixelSize: Theme.fontSizeLarge
                                color: Theme.highlightColor
                            }
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: engine.roundResult
                                font.pixelSize: Theme.fontSizeSmall
                            }
                            Label {
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                                text: qsTr("Match: %1 : %2").arg(engine.playerGamePoints).arg(engine.cpuGamePoints)
                                color: Theme.secondaryColor
                            }
                            Button {
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

                    Timer {
                        id: pulleyActionTimer
                        interval: 80
                        repeat: false
                        onTriggered: mainPage.tryPulleyAction()
                    }

                    RemorsePopup { id: newMatchRemorse }
                    RemorsePopup { id: closeRemorse }

                    Rectangle {
                        id: noticePanel
                        property alias text: noticeLabel.text
                        visible: false
                        z: 600
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: matchLabel.bottom
                        anchors.topMargin: Theme.paddingLarge
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        height: noticeLabel.height + 2 * Theme.paddingMedium
                        radius: Theme.paddingSmall
                        color: "#f0202020"
                        border.color: Theme.highlightColor

                        Label {
                            id: noticeLabel
                            anchors.centerIn: parent
                            width: parent.width - 2 * Theme.paddingMedium
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.highlightColor
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
            }

            Item { id: animationLayer; anchors.fill: parent; z: 1000 }

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
}
