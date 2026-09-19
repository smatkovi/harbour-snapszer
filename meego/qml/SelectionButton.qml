import QtQuick 1.1
import com.nokia.meego 1.0
import "Style.js" as Style

// A labelled button that opens a selection dialog; the Harmattan
// counterpart of a Silica ComboBox.
Item {
    id: root
    property string label: ""
    property string description: ""
    property variant options: []
    property int currentIndex: 0
    property bool enabled: true
    signal selected(int index)

    width: parent ? parent.width : 480
    height: column.height + Style.paddingMedium

    Column {
        id: column
        x: Style.horizontalPageMargin
        width: parent.width - 2 * Style.horizontalPageMargin
        spacing: Style.paddingSmall

        Text {
            width: parent.width
            text: root.label
            color: Style.primaryColor
            font.pixelSize: Style.fontSizeSmall
        }
        Button {
            width: parent.width
            enabled: root.enabled
            text: root.currentIndex >= 0 && root.currentIndex < root.options.length
                  ? root.options[root.currentIndex] : ""
            onClicked: {
                dialog.model.clear()
                for (var i = 0; i < root.options.length; ++i)
                    dialog.model.append({ "name": root.options[i] })
                dialog.selectedIndex = root.currentIndex
                dialog.open()
            }
        }
        Text {
            width: parent.width
            visible: root.description !== ""
            text: root.description
            wrapMode: Text.WordWrap
            color: Style.secondaryColor
            font.pixelSize: Style.fontSizeExtraSmall
        }
    }

    SelectionDialog {
        id: dialog
        titleText: root.label
        model: ListModel { }
        onAccepted: {
            if (selectedIndex !== root.currentIndex) {
                root.currentIndex = selectedIndex
                root.selected(selectedIndex)
            }
        }
    }
}
