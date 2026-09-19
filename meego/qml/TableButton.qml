import QtQuick 1.1
import com.nokia.meego 1.0
import "Style.js" as Style

// Compact button for the table; Harmattan buttons are otherwise page-wide.
Button {
    width: Math.max(Style.itemSizeLarge * 1.1, text.length * Style.fontSizeSmall * 0.58 + 2 * Style.paddingLarge)
    height: Style.itemSizeSmall * 0.8
}
