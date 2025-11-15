// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import CocktailMaker.Ui

Page {
    id: root

    signal recipeSelected(string name)
    RecipeListModel {
        id: recipeListModel
        recipeStore: ApplicationState.recipeStore
    }

    header: Item {
        implicitHeight: headerLabel.height
        Label {
            id: headerLabel
            text: qsTr("🍹 Cocktail Automat")
            font.pointSize: 48
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    GridView {
        id: gridView
        clip: true
        model: recipeListModel
        anchors.fill: parent
        cellWidth: gridView.width / 2
        cellHeight: cellWidth

        delegate: Item {
            required property string name
            required property string imageSource
            width: gridView.cellWidth
            height: gridView.cellHeight
            ImageCard {
                anchors.margins: 10
                anchors.fill: parent
                clip: true
                text: qsTr(parent.name)
                iconSrc: parent.imageSource
                onClicked: {
                    root.recipeSelected(parent.name);
                }
            }
        }

        ScrollIndicator {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 20
            down: false
            atEnd: gridView.atYBeginning
            movingVertically: gridView.movingVertically
        }

        ScrollIndicator {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 20
            down: true
            atEnd: gridView.atYEnd
            movingVertically: gridView.movingVertically
        }
    }
}
