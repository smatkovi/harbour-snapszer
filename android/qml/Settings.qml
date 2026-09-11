import QtQuick
import QtQuick.Controls

SubPage {
    id: page

    // Root-context engine instance, as on Sailfish OS.
    property var settings: snapszerEngine

    title: qsTr("Settings")

    SectionLabel { text: qsTr("Players") }

    TextBlock { text: qsTr("Your name"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    TextField {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        placeholderText: qsTr("Player")
        text: page.settings.playerName
        onTextEdited: page.settings.playerName = text
    }

    TextBlock { text: qsTr("Opponent name"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    TextField {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        placeholderText: qsTr("AI")
        text: page.settings.opponentName
        onTextEdited: page.settings.opponentName = text
    }

    SectionLabel { text: qsTr("Cards") }

    TextBlock { text: qsTr("Card style"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    ComboBox {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        model: ["Piatnik", "Betyár"]
        currentIndex: page.settings.cardStyle === "Betyar" ? 1 : 0
        onActivated: (index) => page.settings.cardStyle = index === 1 ? "Betyar" : "Piatnik"
    }

    SectionLabel { text: qsTr("Opponent") }

    TextBlock { text: qsTr("Difficulty"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    ComboBox {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        model: [qsTr("Easy"), qsTr("Normal"), qsTr("Hard"), qsTr("Expert")]
        currentIndex: page.settings.aiDifficulty
        onActivated: (index) => page.settings.aiDifficulty = index
    }

    TextBlock {
        text: qsTr("Opponent pace") + ": " + (paceSlider.value < 400 ? qsTr("Fast")
                                              : paceSlider.value < 900 ? qsTr("Normal") : qsTr("Slow"))
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
    }
    Slider {
        id: paceSlider
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        from: 150
        to: 1500
        stepSize: 50
        value: page.settings.aiPlayDelay
        onMoved: page.settings.aiPlayDelay = Math.round(value)
    }

    SectionLabel { text: qsTr("Three and four players") }

    TextBlock { text: qsTr("Rules for 3 players"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    ComboBox {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        model: [multiEngine.rulesNameFor(3, 0), multiEngine.rulesNameFor(3, 1)]
        currentIndex: multiEngine.rules3
        onActivated: (index) => multiEngine.rules3 = index
    }

    TextBlock { text: qsTr("Rules for 4 players"); color: Theme.secondaryColor; font.pixelSize: Theme.fontSizeExtraSmall }
    ComboBox {
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        model: [multiEngine.rulesNameFor(4, 0), multiEngine.rulesNameFor(4, 1)]
        currentIndex: multiEngine.rules4
        onActivated: (index) => multiEngine.rules4 = index
    }
    TextBlock {
        text: qsTr("Applies to the next new match")
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
    }

    SectionLabel { text: qsTr("Animation") }

    Switch {
        x: Theme.horizontalPageMargin
        text: qsTr("Enable animations")
        checked: page.settings.animationsEnabled
        onToggled: page.settings.animationsEnabled = checked
    }
    TextBlock {
        text: qsTr("Turn off for immediate card movement")
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
    }

    TextBlock {
        text: qsTr("Card speed") + ": " + (speedSlider.value <= 0.75 ? qsTr("Slow")
                                           : speedSlider.value <= 1.25 ? qsTr("Normal") : qsTr("Fast"))
        color: Theme.secondaryColor
        font.pixelSize: Theme.fontSizeExtraSmall
    }
    Slider {
        id: speedSlider
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        from: 0.5
        to: 2.0
        stepSize: 0.25
        value: page.settings.animationSpeed
        enabled: page.settings.animationsEnabled
        onMoved: page.settings.animationSpeed = value
    }

    Button {
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("Reset to defaults")
        onClicked: {
            page.settings.playerName = "Player"
            page.settings.opponentName = "AI"
            page.settings.cardStyle = "Piatnik"
            page.settings.aiDifficulty = 1
            page.settings.aiPlayDelay = 650
            page.settings.animationsEnabled = true
            page.settings.animationSpeed = 1.0
        }
    }
}
