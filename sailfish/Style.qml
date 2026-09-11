/*
    Copyright (C) 2026 edp17 and chatGPT

    This file is part of harbour-snapszer.

    The harbour-snapszer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    The harbour-snapszer is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the harbour-snapszer. If not, see <http://www.gnu.org/licenses/>.
*/
pragma Singleton
import QtQuick 2.6
import Sailfish.Silica 1.0

// Silica metrics under a neutral name for the QML shared with the Android
// build (qml-common).
QtObject {
    readonly property real paddingSmall: Theme.paddingSmall
    readonly property real paddingMedium: Theme.paddingMedium
    readonly property real paddingLarge: Theme.paddingLarge
    readonly property real horizontalPageMargin: Theme.horizontalPageMargin
    readonly property real itemSizeSmall: Theme.itemSizeSmall
    readonly property real itemSizeMedium: Theme.itemSizeMedium
    readonly property real itemSizeLarge: Theme.itemSizeLarge
    readonly property real fontSizeTiny: Theme.fontSizeTiny
    readonly property real fontSizeExtraSmall: Theme.fontSizeExtraSmall
    readonly property real fontSizeSmall: Theme.fontSizeSmall
    readonly property real fontSizeMedium: Theme.fontSizeMedium
    readonly property real fontSizeLarge: Theme.fontSizeLarge
    readonly property color primaryColor: Theme.primaryColor
    readonly property color secondaryColor: Theme.secondaryColor
    readonly property color highlightColor: Theme.highlightColor
    readonly property color secondaryHighlightColor: Theme.secondaryHighlightColor
}
