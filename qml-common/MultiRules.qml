/*
    Rules of the three- and four-player games, shared by both builds.
*/
import QtQuick 2.6
import "."

Column {
    id: rules
    width: parent ? parent.width : 400
    spacing: Style.paddingSmall

    function block(title, body) {
        return { "title": title, "body": body }
    }

    Repeater {
        model: [
            rules.block(qsTr("Three and four players"),
                        qsTr("Choose the rules for three and for four players in the settings. In all four games you must follow suit and beat the highest card of the trick when you can; without the led suit you must trump, and overtrump if possible. Card values are the same as in the two-player game; the nine counts 0. A match is played to 24 points.")),
            rules.block(qsTr("Hungarian three-player snapszer"),
                        qsTr("24 cards with the nines, eight cards each, no talon. The player after the dealer (hívó) names trump after seeing four cards and plays alone against the other two, who count their tricks together. Before the first card he may announce a Snapszer (6 points): he must reach 66 without the others taking a trick. Otherwise the first side to reach 66 calls it: 1 point, 2 if the losers have fewer than 33, 3 if they took no trick. Defenders may double (Kontra), the hívó may redouble, and the defenders once more.")),
            rules.block(qsTr("Hungarian four-player snapszer"),
                        qsTr("24 cards, six each. After three cards the hívó calls any card; its suit is trump and whoever holds it is his secret partner. Until the called card is played only the hívó may call 66, and then he wins alone. After that the two sides count together. Snapszer (6), Kontra and the scoring are as with three players; every member of the winning side scores.")),
            rules.block(qsTr("Dreierschnapsen"),
                        qsTr("20 cards, six each and two in the talon. The player after the dealer names trump from his first three cards and must at least play the normal game. Everybody may outbid him: Bettler 4 (take no trick, no trump), Schnapser 6 (forehand: 66 within the first four tricks, taking all of them), Gang 9 (all tricks, no trump), Zehnergang 10 (all tricks, ten high, ace low), Kontraschnapser 12 (Schnapser by another player), Bauernschnapser 12 (forehand takes all tricks with trump) and Kontrabauernschnapser 24. The declarer takes the talon, discards two cards and plays alone; if he fails, both others score.")),
            rules.block(qsTr("Bauernschnapsen"),
                        qsTr("20 cards, five each, partners sit opposite. Bidding as in Dreierschnapsen without a talon. Schnapser and Bauernschnapser are for the forehand's side, the Kontra games for the other side. In Bettler, Gang and Zehnergang the declarer's partner sits out. In Schnapser and all-trick games the declarer himself must take the tricks; a trick of his partner loses. Both partners score together."))
        ]

        Column {
            width: rules.width
            spacing: Style.paddingSmall / 2

            Text {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.WordWrap
                text: modelData.title
                color: Style.highlightColor
                font.pixelSize: Style.fontSizeSmall
            }
            Text {
                x: Style.horizontalPageMargin
                width: parent.width - 2 * Style.horizontalPageMargin
                wrapMode: Text.WordWrap
                text: modelData.body
                color: Style.primaryColor
                font.pixelSize: Style.fontSizeSmall
            }
        }
    }
}
