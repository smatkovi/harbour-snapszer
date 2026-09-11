import QtQuick
import QtQuick.Controls

// Scrollable page with a back button, used for everything but the table.
Page {
    id: page
    default property alias content: column.data

    background: Rectangle { color: Theme.tableColor }

    header: ToolBar {
        background: Rectangle { color: Theme.barColor }
        Row {
            anchors.fill: parent
            spacing: Theme.paddingSmall
            ToolButton {
                text: "←"
                font.pixelSize: Theme.fontSizeLarge
                onClicked: page.StackView.view.pop()
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - x - Theme.paddingLarge
                text: page.title
                font.pixelSize: Theme.fontSizeMedium
                elide: Text.ElideRight
            }
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge
        clip: true
        ScrollIndicator.vertical: ScrollIndicator { }

        Column {
            id: column
            width: page.width
            topPadding: Theme.paddingMedium
            spacing: Theme.paddingMedium
        }
    }
}
