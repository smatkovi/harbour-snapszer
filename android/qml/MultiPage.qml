import QtQuick
import QtQuick.Controls

Item {
    id: page
    objectName: "multiPage"

    property var engine: multiEngine

    StackView.onStatusChanged: engine.paused = StackView.status !== StackView.Active

    Connections {
        target: page.engine
        // A LAN match that ended (host left, connection lost) closes the table.
        function onStateChanged() {
            const view = page.StackView.view
            if (!page.engine.active && view && view.currentItem === page)
                view.pop()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.tableColor
    }

    MultiTable {
        anchors.fill: parent
    }

    ToolButton {
        id: menuButton
        anchors.top: parent.top
        anchors.right: parent.right
        text: "⋮"
        font.pixelSize: Theme.fontSizeLarge
        z: 2000
        onClicked: menu.popup(menuButton, 0, menuButton.height)
    }

    Menu {
        id: menu
        MenuItem {
            text: qsTranslate("MultiPage", "Leave LAN game")
            visible: page.engine.networkGame
            height: visible ? implicitHeight : 0
            onTriggered: confirm.execute(qsTranslate("MultiPage", "Leaving the LAN game"), function() { page.engine.cancelLan() })
        }
        MenuItem {
            text: qsTranslate("MultiPage", "New match")
            visible: !page.engine.lanGuest
            height: visible ? implicitHeight : 0
            onTriggered: confirm.execute(qsTranslate("MultiPage", "Starting a new match"), function() { page.engine.newMatch() })
        }
        MenuItem {
            text: qsTranslate("MultiPage", "Two-player table")
            onTriggered: page.StackView.view.pop()
        }
        MenuItem {
            text: qsTranslate("MultiPage", "How to play")
            onTriggered: page.StackView.view.push(Qt.resolvedUrl("GameRules.qml"), { "cardStyle": snapszerEngine.cardStyle })
        }
        MenuItem {
            text: qsTranslate("MultiPage", "Settings")
            onTriggered: page.StackView.view.push(Qt.resolvedUrl("Settings.qml"))
        }
    }

    Dialog {
        id: confirm
        property var confirmed: null
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: if (confirmed) confirmed()

        function execute(text, callback) {
            title = text
            confirmed = callback
            open()
        }
    }
}
