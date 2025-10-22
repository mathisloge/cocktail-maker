// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import CocktailMaker.Ui

Item {
    id: root

    signal recipeSelected(string name)
    RecipeListModel {
        id: recipeListModel
        recipeStore: ApplicationState.recipeStore
    }

    SystemPalette {
        id: systemPalette
    }

    RowLayout {
        id: headerLayout
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        Text {
            text: "🍹 Cocktail Automat"
            font.family: "Helvetica"
            font.pointSize: 48
            color: systemPalette.text

            Layout.alignment: Qt.AlignHCenter
        }
    }
    ScrollView {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerLayout.bottom

        GridLayout {
            id: cocktailGrid
            columns: 2
            columnSpacing: 20
            rowSpacing: 20
            width: root.width
        }

        Instantiator {
            id: cocktailInstantiator
            model: recipeListModel

            delegate: ImageCard {
                required property var model
                clip: true
                Layout.preferredWidth: (cocktailGrid.width - ((cocktailGrid.columns - 1) * cocktailGrid.columnSpacing)) / cocktailGrid.columns
                Layout.preferredHeight: width
                text: model.name
                iconSrc: model.imageSource

                parent: cocktailGrid
                onClicked: {
                    root.recipeSelected(model.name)
                }
            }
        }
    }
}
