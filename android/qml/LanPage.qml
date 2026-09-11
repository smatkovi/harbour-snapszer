import QtQuick
import QtQuick.Controls

SubPage {
    id: page

    property var engine: snapszerEngine

    title: qsTr("LAN game")

    Component.onCompleted: engine.discoverLanHosts()
    // Leaving this page without a started game cancels hosting or joining.
    Component.onDestruction: {
        if (engine && !engine.networkGame)
            engine.cancelLan()
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
        text: qsTr("Both phones need Snapszer and must be connected to the same Wi-Fi network; a hotspot opened by one of the phones works too. Your game against the AI is kept and continues afterwards.")
    }

    Row {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        spacing: Theme.paddingMedium
        visible: page.engine.networkStatus !== "" || page.engine.lanBusy

        BusyIndicator {
            id: busy
            running: page.engine.lanBusy
            visible: running
            width: Theme.itemSizeExtraSmall * 0.6
            height: width
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            width: parent.width - (busy.visible ? busy.width + parent.spacing : 0)
            anchors.verticalCenter: parent.verticalCenter
            wrapMode: Text.WordWrap
            text: page.engine.networkStatus
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
        }
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        visible: page.engine.lanBusy
        text: qsTr("Cancel")
        onClicked: page.engine.cancelLan()
    }

    SectionLabel { text: qsTr("Host a game") }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        enabled: !page.engine.lanBusy
        text: qsTr("Host")
        onClicked: page.engine.hostLanGame()
    }

    TextBlock {
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
        text: page.engine.localAddresses !== ""
              ? qsTr("Address of this phone: %1").arg(page.engine.localAddresses)
              : qsTr("This phone is not connected to a network")
    }

    SectionLabel { text: qsTr("Join a game") }

    TextBlock {
        visible: page.engine.discoveredHosts.length === 0
        color: Theme.secondaryColor
        text: qsTr("No hosted games found yet. Start hosting on the other phone, then search again.")
    }

    Repeater {
        model: page.engine.discoveredHosts
        ItemDelegate {
            width: page.width
            enabled: !page.engine.lanBusy
            text: modelData.name + "  •  " + modelData.address
            leftPadding: Theme.horizontalPageMargin
            onClicked: page.engine.joinLanGame(modelData.address)
        }
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        enabled: !page.engine.lanBusy
        text: qsTr("Search again")
        onClicked: page.engine.discoverLanHosts()
    }

    TextBlock { text: qsTr("Address of the hosting phone"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    TextField {
        id: addressField
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        placeholderText: qsTr("e.g. 192.168.1.23")
        text: page.engine.lanAddress
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhPreferNumbers
        onAccepted: page.engine.joinLanGame(text)
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        enabled: addressField.text.trim().length > 0 && !page.engine.lanBusy
        text: qsTr("Connect")
        onClicked: page.engine.joinLanGame(addressField.text)
    }
}
