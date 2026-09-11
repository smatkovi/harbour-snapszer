import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

ApplicationWindow {
    id: app
    visible: true
    width: 411
    height: 900
    title: "Snapszer"
    color: Theme.tableColor

    Material.theme: Material.Dark
    Material.accent: Theme.highlightColor
    Material.primary: Theme.barColor

    // Android draws the app behind the status and navigation bars.
    Item {
        id: safeFrame
        anchors.fill: parent

        StackView {
            id: stack
            anchors.fill: parent
            anchors.topMargin: safeFrame.SafeArea.margins.top
            anchors.bottomMargin: safeFrame.SafeArea.margins.bottom
            anchors.leftMargin: safeFrame.SafeArea.margins.left
            anchors.rightMargin: safeFrame.SafeArea.margins.right
            initialItem: Table { }
        }
    }

    // Three- and four-player matches (local or LAN) open their own table on
    // top of the two-player one.
    Connections {
        target: multiEngine
        function onMatchStarted() {
            if (stack.currentItem && stack.currentItem.objectName === "multiPage")
                return
            stack.pop(null, StackView.Immediate)
            stack.push(Qt.resolvedUrl("MultiPage.qml"))
        }
    }

    // The Android back button closes pages before it closes the app.
    onClosing: (close) => {
        if (stack.depth > 1) {
            close.accepted = false
            stack.pop()
        }
    }
}
