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
        text: webEdition
              ? "Browser build of harbour-snapszer, the Sailfish OS game by edp17. It plays against the computer and around one device; a browser cannot open the network connections the LAN game needs."
              : "Android build of harbour-snapszer, the Sailfish OS game by edp17, with play over LAN against Sailfish OS and Android phones."
    }

    SectionLabel { text: qsTr("Release") }

    Repeater {
        model: [
            { label: qsTr("Version"), value: "1.1.0" },
            { label: qsTr("Developer"), value: "edp17" },
            { label: qsTr("License"), value: webEdition ? "GPL v3" : "MIT" },
            { label: qsTr("Built with"), value: "Qt " + qtVersion }
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

    TextBlock {
        horizontalAlignment: Text.AlignHCenter
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
        text: webEdition
              ? "The game's own code is offered under the MIT licence. This build links Qt for WebAssembly statically, and that is available only under the GNU General Public License version 3, so the published page as a whole is distributed under the GPL version 3. The source of both is linked below."
              : "The game's own code is offered under the MIT licence. This build uses the Qt framework under the GNU Lesser General Public License version 3."
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("View source on GitHub")
        // This fork carries the Android, browser and N9 editions; edp17's tree
        // has only the Sailfish OS one.
        onClicked: Qt.openUrlExternally("https://github.com/smatkovi/harbour-snapszer")
    }
}
