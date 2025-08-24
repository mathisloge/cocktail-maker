// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Item {
    id: root
    objectName: "recipeDetailPage"
    signal backClicked
    signal mixClicked(RecipeDetail recipe)
    property glass glass
    required property RecipeDetail recipe
    property RecipeDetail __recipe: recipe
    readonly property int sectionWidth: 600

    onRecipeChanged: {
        root.__recipe = recipe;
    }

    RecipeBoosterAdapter {
        id: booster
        ingredientStore: ApplicationState.ingredientStore
        originalRecipe: root.recipe
    }

    ColumnLayout {
        width: parent.width
        spacing: 20

        Item {
            Layout.preferredHeight: headerText.height
            Layout.fillWidth: true
            Button {
                text: "Zurück"
                onClicked: root.backClicked()
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                id: headerText
                text: root.__recipe.name
                font.pointSize: 48
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
        Label {
            text: root.__recipe.description
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 20
            color: "#b3ffffff"
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.width
        }

        Section {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.sectionWidth
            visible: booster.isBoostable

            ColumnLayout {
                width: parent.width
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    font.pointSize: 18
                    text: "Getränk boosten"
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    font.pointSize: 18
                    text: boostSlider.value.toFixed(0) + "%"
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Label {
                        text: "mild"
                    }
                    Slider {
                        id: boostSlider
                        from: -100
                        value: 0
                        to: 100
                        stepSize: 5
                        snapMode: Slider.SnapOnRelease
                        onValueChanged: {
                            root.__recipe = booster.boost(boostSlider.value);
                        }
                    }
                    Label {
                        text: "stark"
                    }
                }
            }
        }

        Section {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.sectionWidth
            ColumnLayout {
                width: parent.width
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    font.pointSize: 18
                    text: "Schritte:"
                }

                Repeater {
                    model: root.__recipe.steps

                    delegate: Section {
                        id: section
                        required property var model
                        Layout.fillWidth: true
                        RowLayout {
                            width: parent.width
                            Label {
                                Layout.alignment: Qt.AlignLeft
                                text: section.model.name
                                font.pointSize: 14
                            }
                            Label {
                                Layout.alignment: Qt.AlignRight
                                color: "#b3ffffff"
                                text: section.model.detail
                                font.pointSize: 14
                            }
                        }
                    }
                }
            }
        }

        Button {
            Layout.alignment: Qt.AlignHCenter
            text: "🍹 mixen!"
            onClicked: root.mixClicked(root.__recipe)
        }
    }
}
