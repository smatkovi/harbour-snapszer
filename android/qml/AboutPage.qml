import QtQuick
import QtQuick.Controls

SubPage {
    title: qsTr("About Snapszer")

    Image {
        width: Theme.itemSizeHuge
        height: width
        anchors.horizontalCenter: parent.horizontalCenter
        source: "qrc:/sailfish/icons/icon-256.png"
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Label {
        width: parent.width
        text: "Snapszer"
        horizontalAlignment: Text.AlignHCenter
        font.pixelSize: Theme.fontSizeHuge
        font.bold: true
        color: Theme.highlightColor
    }

    Label {
        width: parent.width
        text: qsTr("Classic Hungarian 66")
        horizontalAlignment: Text.AlignHCenter
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeSmall
    }

    TextBlock {
        horizontalAlignment: Text.AlignHCenter
        text: "Android build of harbour-snapszer, the Sailfish OS game by edp17, with play over LAN against Sailfish OS and Android phones."
    }

    SectionLabel { text: qsTr("Release") }

    Repeater {
        model: [
            { label: qsTr("Version"), value: "1.1.0" },
            { label: qsTr("Developer"), value: "edp17" },
            { label: qsTr("License"), value: "MIT" }
        ]
        Row {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            Label {
                width: parent.width / 2 - Theme.paddingSmall
                horizontalAlignment: Text.AlignRight
                text: modelData.label
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
            Item { width: Theme.paddingMedium; height: 1 }
            Label {
                text: modelData.value
                font.pixelSize: Theme.fontSizeSmall
            }
        }
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("View source on GitHub")
        onClicked: Qt.openUrlExternally("https://github.com/edp17/harbour-snapszer")
    }
}
