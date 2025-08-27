// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 600
    title: "Cocktail Automat"

    property RecipeDetail selectedRecipe

    SystemPalette {
        id: systemPalette
    }

    Frame {
        anchors.fill: parent
        background: Rectangle {
            color: "#1affffff"
            border.color: "#0dffffff"

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: "#581c87"
                }  // purple-900
                GradientStop {
                    position: 0.8
                    color: "#1e40af"
                }  // blue-800
                GradientStop {
                    position: 1.0
                    color: "#312e81"
                }  // indigo-800
            }
        }

        StackView {
            objectName: "stackView"
            id: stackView
            anchors.fill: parent
            initialItem: cocktailSelectionPage
        }
    }

    Component {
        id: cocktailSelectionPage
        RecipeListPage {
            onRecipeSelected: name => {
                window.selectedRecipe = ApplicationState.recipeFactory.create(name);
                stackView.push(cocktailDetailPage);
            }
        }
    }

    Component {
        id: cocktailDetailPage

        RecipeDetailPage {
            recipe: window.selectedRecipe
            onBackClicked: stackView.pop()
            onMixClicked: (recipe) => {
                window.selectedRecipe = recipe
                stackView.push(glassSelectionPage)
            }
        }
    }

    Component {
        id: glassSelectionPage
        GlassSelectionPage {
            recipe: window.selectedRecipe
            onGlassSelected: (glassId) => {
                ApplicationState.recipeExecutor.make_recipe(window.selectedRecipe, glassId)
                stackView.push(mixingPage)
            }

            onBackClicked: {
                stackView.pop()
            }

        }
    }

    Component {
        id: mixingPage

        MixPage {
            recipe: window.selectedRecipe
            onCancelClicked: stackView.pop()
            onFinished: stackView.pop(null)
        }

    }
}
