import QtQuick
import QtQuick.Controls

SubPage {
    id: page

    property var engine: snapszerEngine
    property var multi: multiEngine
    // Which engine the last host or join went to, for the status line.
    property bool useMulti: false
    readonly property bool busy: engine.lanBusy || multi.lanBusy
    readonly property string statusText: useMulti ? multi.networkStatus : engine.networkStatus
    readonly property string internetAddress: lanBrowser.internetAddresses.split("\n")[0]

    title: qsTr("LAN game")

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
        target: page.engine
        function onLanRedirect(address, players) { page.join(address, players) }
    }
    Connections {
        target: page.multi
        function onLanRedirect(address, players) { page.join(address, players) }
    }

    Connections {
        target: page.engine
        function onNetworkChanged() {
            const view = page.StackView.view
            if (page.engine.networkGame && view && view.currentItem === page)
                view.pop()
        }
    }

    TextBlock {
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeExtraSmall
        text: qsTr("All phones need Snapszer and must be connected to the same Wi-Fi network; a hotspot opened by one of the phones works too. Your games against the computer are kept and continue afterwards.")
    }

    Row {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        spacing: Theme.paddingMedium
        visible: page.statusText !== "" || page.busy

        BusyIndicator {
            id: busyIndicator
            running: page.busy
            visible: running
            width: Theme.itemSizeExtraSmall * 0.6
            height: width
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            width: parent.width - (busyIndicator.visible ? busyIndicator.width + parent.spacing : 0)
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: Text.WordWrap
            text: page.statusText
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Repeater {
        model: page.multi.lanBusy ? page.multi.lobby : []
        TextBlock {
            x: Theme.horizontalPageMargin * 2
            text: qsTr("Seat %1: %2").arg(index + 1).arg(modelData.taken ? modelData.name : qsTr("free (computer)"))
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

    SectionLabel { text: qsTr("Host a game") }

    TextBlock { text: qsTr("Players"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    ComboBox {
        id: playersBox
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        enabled: !page.busy
        model: ["2 – " + qsTr("Classic Snapszer"),
                "3 – " + page.multi.rulesNameFor(3, page.multi.rules3),
                "4 – " + page.multi.rulesNameFor(4, page.multi.rules4)]
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        enabled: !page.busy
        text: qsTr("Host")
        onClicked: page.host(playersBox.currentIndex + 2)
    }

    TextBlock {
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
        text: lanBrowser.localAddresses !== ""
              ? qsTr("Address of this phone: %1").arg(lanBrowser.localAddresses)
              : qsTr("This phone is not connected to a network")
    }

    ItemDelegate {
        width: page.width
        visible: page.internetAddress !== ""
        leftPadding: Theme.horizontalPageMargin
        rightPadding: Theme.horizontalPageMargin
        contentItem: Column {
            spacing: 2
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
                text: page.internetAddress
            }
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.secondaryColor
                text: qsTr("Works when both networks support IPv6 and allow incoming connections. Over IPv4 the host must be reachable by other means, e.g. a VPN.")
            }
        }
        onClicked: lanBrowser.copyToClipboard(page.internetAddress)
    }

    SectionLabel { text: qsTr("Join a game") }

    TextBlock {
        visible: lanBrowser.hosts.length === 0
        color: Theme.secondaryColor
        text: lanBrowser.searching ? qsTr("Searching…")
                                   : qsTr("No hosted games found yet. Start hosting on the other phone, then search again.")
    }

    Repeater {
        model: lanBrowser.hosts
        ItemDelegate {
            width: page.width
            enabled: !page.busy
            leftPadding: Theme.horizontalPageMargin
            text: modelData.name + "\n" + qsTr("%1 players · %2 free · %3").arg(modelData.players)
                  .arg(modelData.openSeats).arg(modelData.address)
            onClicked: page.join(modelData.address, modelData.players)
        }
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        enabled: !page.busy && !lanBrowser.searching
        text: qsTr("Search again")
        onClicked: lanBrowser.search()
    }

    TextBlock { text: qsTr("Address of the hosting phone"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    TextField {
        id: addressField
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        placeholderText: qsTr("e.g. 192.168.1.23 or an IPv6 address")
        text: page.engine.lanAddress
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
        onAccepted: {
            if (text.trim().length > 0 && !page.busy)
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
