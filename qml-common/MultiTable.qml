/*
    Table for three- and four-player Schnapsen, shared by the Sailfish OS and
    Android builds. Kept to QtQuick 2.6 and ES5 so it runs on both Qt 5.6
    and Qt 6; the platform provides Style, Card, FlyingCard and TableButton.
*/
import QtQuick 2.6
import "."

Item {
    id: table

    property var engine: multiEngine
    property string cardStyle: snapszerEngine.cardStyle
    property var hiddenIds: ({})
    property int flightsRemaining: 0
    property int pendingIndex: -1
    property int pendingMarriage: 0

    readonly property int players: engine.players
    readonly property var seatList: engine.seats
    readonly property color partnerColor: "#7fd67f"
    readonly property color opponentColor: "#ff8a80"

    function dur(ms) {
        return Math.max(1, Math.round(ms / Math.max(0.5, snapszerEngine.animationSpeed)))
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
        hiddenIds = ({})
        flightsRemaining = 0
        marriagePanel.visible = false
    }

    // Seat 0 sits at the bottom; the others go clockwise around the table.
    function seatPanel(seat) {
        if (players === 3)
            return seat === 1 ? leftSeat : rightSeat
        return seat === 1 ? leftSeat : seat === 2 ? topSeat : rightSeat
    }

    function seatCenter(seat) {
        if (seat === 0)
            return handArea.mapToItem(animationLayer, handArea.width / 2, handArea.height / 2)
        var panel = seatPanel(seat)
        return panel.mapToItem(animationLayer, panel.width / 2, panel.height / 2)
    }

    function trickSlot(seat) {
        var cx = trickArea.width / 2
        var cy = trickArea.height / 2
        var dx = trickArea.width * 0.28
        var dy = trickArea.height * 0.25
        var x = cx, y = cy
        if (seat === 0) {
            y = cy + dy
        } else if (players === 3) {
            x = seat === 1 ? cx - dx : cx + dx
            y = cy - dy * 0.6
        } else if (seat === 1) {
            x = cx - dx
        } else if (seat === 2) {
            y = cy - dy
        } else {
            x = cx + dx
        }
        return Qt.point(x, y)
    }

    function slotRotation(seat) {
        return [-3, 8, -6, 5][seat % 4]
    }

    function handCardWidth(count) {
        var maxWidth = Style.itemSizeLarge * 1.02
        var available = handArea.width - 2 * Style.paddingSmall
        if (count <= 1)
            return Math.min(maxWidth, available)
        return Math.min(maxWidth, available / (1 + 0.5 * (count - 1)))
    }

    function handCardX(index, count, width) {
        var total = width + Math.max(0, count - 1) * width * 0.5
        return (handArea.width - total) / 2 + index * width * 0.5
    }

    function spawnFlight(id, fromPoint, toPoint, delay, rotation, callback) {
        var object = flyingComponent.createObject(animationLayer, {
            "cardId": id,
            "cardStyle": table.cardStyle,
            "faceUp": true,
            "fromX": fromPoint.x,
            "fromY": fromPoint.y,
            "toX": toPoint.x,
            "toY": toPoint.y,
            "startDelay": delay,
            "flightDuration": dur(320),
            "toRotation": rotation
        })
        if (!object)
            return null
        object.finished.connect(function() { callback(object) })
        return object
    }

    function onCardFlight(cardId, seat) {
        if (!snapszerEngine.animationsEnabled)
            return
        var target = trickArea.mapToItem(animationLayer, trickSlot(seat).x, trickSlot(seat).y)
        hiddenIds = mapSet(hiddenIds, cardId, true)
        var flight = spawnFlight(cardId, seatCenter(seat), target, 0, slotRotation(seat), function(obj) {
            hiddenIds = mapSet(hiddenIds, obj.cardId, false)
            obj.destroy()
            engine.completeCardAnimation()
        })
        if (!flight) {
            hiddenIds = ({})
            engine.completeCardAnimation()
        }
    }

    function onTrickFlight(winnerSeat) {
        if (!snapszerEngine.animationsEnabled)
            return
        var cards = engine.trick
        flightsRemaining = cards.length
        var end = seatCenter(winnerSeat)
        for (var i = 0; i < cards.length; ++i) {
            var slot = trickSlot(cards[i].seat)
            hiddenIds = mapSet(hiddenIds, cards[i].id, true)
            var flight = spawnFlight(cards[i].id, trickArea.mapToItem(animationLayer, slot.x, slot.y), end,
                                     i * dur(60), 0, function(obj) {
                obj.destroy()
                flightsRemaining--
                if (flightsRemaining <= 0) {
                    hiddenIds = ({})
                    engine.completeTrickAnimation()
                }
            })
            if (!flight)
                flightsRemaining--
        }
        if (flightsRemaining <= 0) {
            hiddenIds = ({})
            engine.completeTrickAnimation()
        }
    }

    function onReset() {
        clearFlights()
    }

    function tapCard(card) {
        if (!card.playable)
            return
        if (engine.discarding) {
            engine.act("discard", card.key, false)
        } else if (card.marriage > 0) {
            pendingIndex = card.key
            pendingMarriage = card.marriage
            marriagePanel.visible = true
        } else {
            engine.act("play", card.key, false)
        }
    }

    Component.onCompleted: {
        engine.cardAnimationRequested.connect(onCardFlight)
        engine.trickAnimationRequested.connect(onTrickFlight)
        engine.resetVisuals.connect(onReset)
    }
    Component.onDestruction: {
        engine.cardAnimationRequested.disconnect(onCardFlight)
        engine.trickAnimationRequested.disconnect(onTrickFlight)
        engine.resetVisuals.disconnect(onReset)
    }

    // --- opponents --------------------------------------------------------------

    Component {
        id: seatComponent

        Item {
            id: seatItem
            property int seat: 1
            property var info: seat < table.seatList.length ? table.seatList[seat] : ({})
            width: table.width * (table.players === 3 ? 0.44 : 0.31)
            height: seatColumn.height

            Rectangle {
                anchors.fill: parent
                anchors.margins: -Style.paddingSmall / 2
                radius: Style.paddingSmall
                color: seatItem.info.actor ? "#33ffffff" : "transparent"
                border.width: seatItem.info.party === "partner" || seatItem.info.party === "opponent" ? 2 : 0
                border.color: seatItem.info.party === "partner" ? table.partnerColor : table.opponentColor
            }

            Column {
                id: seatColumn
                width: parent.width
                spacing: 2

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: (seatItem.info.dealer ? "● " : "") + (seatItem.info.name || "")
                    color: seatItem.info.actor ? Style.highlightColor : Style.primaryColor
                    font.pixelSize: Style.fontSizeExtraSmall
                    font.bold: seatItem.info.declarer === true
                }

                Item {
                    width: parent.width
                    height: Style.itemSizeSmall * 0.62
                    opacity: seatItem.info.sittingOut ? 0.35 : 1

                    Repeater {
                        model: Math.min(seatItem.info.cards || 0, 8)
                        Card {
                            property int count: Math.min(seatItem.info.cards || 0, 8)
                            width: Style.itemSizeSmall * 0.38
                            height: width * 1.4
                            x: (parent.width - width - (count - 1) * width * 0.34) / 2 + index * width * 0.34
                            cardId: "0_10"
                            cardStyle: table.cardStyle
                            faceUp: false
                        }
                    }
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    color: Style.secondaryColor
                    font.pixelSize: Style.fontSizeTiny
                    text: {
                        var parts = []
                        if (seatItem.info.declarer)
                            parts.push(qsTr("declarer"))
                        if (seatItem.info.sittingOut)
                            parts.push(qsTr("sits out"))
                        if (seatItem.info.passed)
                            parts.push(qsTr("passed"))
                        if (seatItem.info.party === "partner")
                            parts.push(qsTr("partner"))
                        parts.push(qsTr("%1 tricks").arg(seatItem.info.tricks || 0))
                        parts.push(qsTr("%1 pts").arg(seatItem.info.score || 0))
                        return parts.join(" · ")
                    }
                }
            }
        }
    }

    Text {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Style.horizontalPageMargin
        anchors.rightMargin: Style.horizontalPageMargin * 2.5
        anchors.topMargin: Style.paddingSmall
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: Style.secondaryHighlightColor
        font.pixelSize: Style.fontSizeExtraSmall
        text: engine.rulesName + (engine.networkGame ? "  ·  LAN" : "")
    }

    Loader {
        id: leftSeat
        anchors.left: parent.left
        anchors.leftMargin: Style.paddingSmall
        anchors.top: header.bottom
        anchors.topMargin: table.players === 4 ? Style.itemSizeSmall * 1.1 : Style.paddingLarge
        sourceComponent: seatComponent
        onLoaded: item.seat = 1
    }

    Loader {
        id: topSeat
        active: table.players === 4
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: header.bottom
        anchors.topMargin: Style.paddingSmall
        sourceComponent: seatComponent
        onLoaded: item.seat = 2
    }

    Loader {
        id: rightSeat
        anchors.right: parent.right
        anchors.rightMargin: Style.paddingSmall
        anchors.top: leftSeat.top
        sourceComponent: seatComponent
        // Bound, not assigned: the same table may switch between 3 and 4 players.
        onLoaded: item.seat = Qt.binding(function() { return table.players === 4 ? 3 : 2 })
    }

    // --- trick, trump and status ----------------------------------------------------

    // Fills the space between the opponents and the status lines, so that the
    // cards of opposite seats do not cover each other on tall screens.
    Item {
        id: trickArea
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: leftSeat.bottom
        anchors.topMargin: Style.paddingMedium
        anchors.bottom: infoRow.top
        anchors.bottomMargin: Style.paddingSmall
        width: parent.width * 0.86

        readonly property real cardHeight: Math.min(Style.itemSizeLarge * 1.2, height * 0.46)

        Repeater {
            model: engine.trick
            Card {
                property point slot: table.trickSlot(modelData.seat)
                height: trickArea.cardHeight
                width: height / 1.4
                x: slot.x - width / 2
                y: slot.y - height / 2
                rotation: table.slotRotation(modelData.seat)
                cardId: modelData.id
                cardStyle: table.cardStyle
                faceUp: true
                opacity: table.hiddenIds[modelData.id] ? 0 : 1
                z: index
            }
        }

        // Dreierschnapsen talon while it is still on the table.
        Row {
            anchors.centerIn: parent
            spacing: -Style.itemSizeSmall * 0.2
            visible: engine.phase <= 2 && engine.variant === 1
            Repeater {
                model: 2
                Card {
                    width: Style.itemSizeSmall * 0.5
                    height: width * 1.4
                    cardId: "0_10"
                    cardStyle: table.cardStyle
                    faceUp: false
                }
            }
        }
    }

    Row {
        id: infoRow
        anchors.bottom: statusLabel.top
        anchors.bottomMargin: Style.paddingSmall
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: Style.paddingMedium
        height: Style.itemSizeSmall * 0.55

        Card {
            visible: engine.calledCard !== ""
            height: parent.height
            width: height / 1.4
            cardId: engine.calledCard !== "" ? engine.calledCard : "0_14"
            cardStyle: table.cardStyle
            faceUp: true
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Style.highlightColor
            font.pixelSize: Style.fontSizeExtraSmall
            text: {
                var suits = [qsTr("Bells"), qsTr("Leaves"), qsTr("Acorns"), qsTr("Hearts")]
                var parts = []
                if (engine.trumpSuit >= 0)
                    parts.push(qsTr("Trump: %1").arg(suits[engine.trumpSuit]))
                else if (engine.contractName !== "")
                    parts.push(qsTr("No trump"))
                if (engine.contractName !== "")
                    parts.push(engine.contractName)
                return parts.join("  ·  ")
            }
        }
    }

    Text {
        id: statusLabel
        anchors.bottom: decisionArea.top
        anchors.bottomMargin: Style.paddingSmall
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Style.horizontalPageMargin
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        color: Style.primaryColor
        font.pixelSize: Style.fontSizeSmall
        text: engine.discarding ? qsTr("Tap two cards to put them away") : engine.status
    }

    // --- decisions ----------------------------------------------------------------------

    // Keeps a minimum height so the table does not jump when buttons appear.
    Item {
        id: decisionArea
        anchors.bottom: myInfo.top
        anchors.bottomMargin: Style.paddingSmall
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.max(decisionFlow.height, Style.itemSizeSmall * 0.8)

        Flow {
            id: decisionFlow
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Style.paddingMedium
            anchors.rightMargin: Style.paddingMedium
            spacing: Style.paddingSmall

            Repeater {
                model: engine.options
                TableButton {
                    text: modelData.label
                    onClicked: engine.act(modelData.type, modelData.value, false)
                }
            }

            TableButton {
                visible: engine.canClaim
                text: qsTr("66!")
                onClicked: engine.claim()
            }
        }
    }

    Text {
        id: myInfo
        anchors.bottom: handArea.top
        anchors.bottomMargin: Style.paddingSmall
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * Style.horizontalPageMargin
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: table.seatList.length && table.seatList[0].actor ? Style.highlightColor : Style.secondaryColor
        font.pixelSize: Style.fontSizeExtraSmall
        text: {
            if (!table.seatList.length)
                return ""
            var me = table.seatList[0]
            var parts = [me.name]
            if (me.dealer)
                parts.push(qsTr("dealer"))
            if (me.declarer)
                parts.push(qsTr("declarer"))
            if (me.sittingOut)
                parts.push(qsTr("sits out"))
            parts.push(qsTr("%1 tricks").arg(me.tricks))
            parts.push(qsTr("%1 pts").arg(me.score))
            return parts.join(" · ")
        }
    }

    Item {
        id: handArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Style.paddingSmall
        height: Style.itemSizeLarge * 1.5

        Repeater {
            model: engine.hand
            Item {
                property real cw: table.handCardWidth(engine.hand.length)
                width: cw
                height: cw * 1.4
                x: table.handCardX(index, engine.hand.length, cw)
                y: handArea.height - height
                z: cardMouse.pressed ? 100 : index
                opacity: modelData.playable ? 1.0 : 0.55

                Card {
                    anchors.fill: parent
                    cardId: modelData.id
                    cardStyle: table.cardStyle
                    faceUp: true
                    pressed: cardMouse.pressed
                }

                MouseArea {
                    id: cardMouse
                    anchors.fill: parent
                    enabled: modelData.playable && !marriagePanel.visible
                    onClicked: table.tapCard(modelData)
                }
            }
        }
    }

    // --- overlays -------------------------------------------------------------------------

    Rectangle {
        id: marriagePanel
        visible: false
        z: 500
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: handArea.top
        anchors.bottomMargin: Style.paddingSmall
        width: parent.width - 2 * Style.horizontalPageMargin
        height: marriageColumn.height + 2 * Style.paddingMedium
        radius: Style.paddingSmall
        color: "#ee202020"
        border.color: Style.highlightColor

        Column {
            id: marriageColumn
            anchors.centerIn: parent
            spacing: Style.paddingSmall
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Declare %1?").arg(table.pendingMarriage)
                color: Style.highlightColor
                font.pixelSize: Style.fontSizeSmall
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Style.paddingMedium
                TableButton {
                    text: qsTr("Declare")
                    onClicked: {
                        marriagePanel.visible = false
                        engine.act("play", table.pendingIndex, true)
                    }
                }
                TableButton {
                    text: qsTr("Play only")
                    onClicked: {
                        marriagePanel.visible = false
                        engine.act("play", table.pendingIndex, false)
                    }
                }
            }
        }
    }

    Rectangle {
        id: callPanel
        visible: engine.choosingCard
        z: 480
        anchors.centerIn: parent
        width: parent.width - 2 * Style.paddingMedium
        height: callColumn.height + 2 * Style.paddingMedium
        radius: Style.paddingSmall
        color: "#f0202020"
        border.color: Style.highlightColor

        Column {
            id: callColumn
            anchors.centerIn: parent
            width: parent.width - 2 * Style.paddingMedium
            spacing: Style.paddingSmall

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("Call a card. Its suit becomes trump and whoever holds it is your secret partner.")
                color: Style.highlightColor
                font.pixelSize: Style.fontSizeExtraSmall
            }

            Grid {
                anchors.horizontalCenter: parent.horizontalCenter
                columns: 6
                spacing: Style.paddingSmall / 2
                Repeater {
                    model: 24
                    Card {
                        property int suit: Math.floor(index / 6)
                        property int rank: [14, 10, 13, 12, 11, 9][index % 6]
                        width: (callColumn.width - 5 * Style.paddingSmall / 2) / 6
                        height: width * 1.4
                        cardId: suit + "_" + rank
                        cardStyle: table.cardStyle
                        faceUp: true
                        MouseArea {
                            anchors.fill: parent
                            onClicked: engine.act("call", parent.suit * 100 + parent.rank, false)
                        }
                    }
                }
            }
        }
    }

    Rectangle {
        id: resultPanel
        visible: engine.roundOver
        z: 450
        anchors.centerIn: parent
        width: parent.width - 2 * Style.horizontalPageMargin
        height: resultColumn.height + 2 * Style.paddingLarge
        radius: Style.paddingSmall
        color: "#f0202020"
        border.color: Style.highlightColor

        Column {
            id: resultColumn
            anchors.centerIn: parent
            width: parent.width - 2 * Style.paddingLarge
            spacing: Style.paddingMedium

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: engine.matchOver ? qsTr("Match over") : qsTr("Round over")
                color: Style.highlightColor
                font.pixelSize: Style.fontSizeLarge
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: engine.roundResult
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }
            Repeater {
                model: table.seatList
                Text {
                    width: resultColumn.width
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData.name + ":  " + modelData.score
                    color: modelData.winner ? Style.highlightColor : Style.secondaryColor
                    font.pixelSize: Style.fontSizeSmall
                }
            }
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("The match is played to %1 points.").arg(24)
                color: Style.secondaryColor
                font.pixelSize: Style.fontSizeTiny
            }
            TableButton {
                anchors.horizontalCenter: parent.horizontalCenter
                text: engine.matchOver ? qsTr("New match") : qsTr("Next round")
                onClicked: {
                    table.clearFlights()
                    if (engine.matchOver)
                        engine.newMatch()
                    else
                        engine.nextRound()
                }
            }
        }
    }

    Item {
        id: animationLayer
        anchors.fill: parent
        z: 1000
    }

    Component {
        id: flyingComponent
        FlyingCard { }
    }
}
