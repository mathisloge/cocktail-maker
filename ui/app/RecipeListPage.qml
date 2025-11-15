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

    SystemPalette {
        id: systemPalette
    }

    header: Item {
        implicitHeight: childrenRect.height
        Label {
            text: qsTr("🍹 Cocktail Automat")
            font.pointSize: 48
            anchors.horizontalCenter: parent.horizontalCenter
        }
    }

    ScrollView {
        anchors.fill: parent

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
                text: qsTr(model.name)
                iconSrc: model.imageSource

                parent: cocktailGrid
                onClicked: {
                    root.recipeSelected(model.name);
                }
            }
        }
    }
}
