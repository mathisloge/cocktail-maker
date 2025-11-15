// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Page {
    id: root
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

    header: Item {
        implicitHeight: headerLabel.height

        Button {
            anchors {
                verticalCenter: parent.verticalCenter
                left: parent.left
            }
            width: 150
            text: qsTr("Zurück")
            onClicked: root.backClicked()
        }

        Label {
            id: headerLabel
            anchors {
                verticalCenter: parent.verticalCenter
                horizontalCenter: parent.horizontalCenter
            }
            text: qsTr(root.__recipe.name)
            font.pointSize: 48
            Layout.alignment: Qt.AlignHCenter
        }

        Button {
            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
            }
            width: 150
            text: qsTr("🍹 Weiter!")
            onClicked: root.mixClicked(root.__recipe)
        }
    }

    ScrollView {
        anchors.fill: parent
        ColumnLayout {
            width: parent.width
            spacing: 20

            Label {
                text: qsTr(root.__recipe.description)
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
                        text: qsTr("Getränk boosten")
                    }

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        font.pointSize: 18
                        text: boostSlider.value.toFixed(0) + "%"
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Label {
                            text: qsTr("mild")
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
                            text: qsTr("stark")
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
                        font.pointSize: 18
                        text: qsTr("Was drin ist") // codespell:ignore
                    }

                    Repeater {
                        model: root.__recipe.steps

                        delegate: Section {
                            id: section
                            required property string name
                            required property string detail
                            Layout.fillWidth: true
                            RowLayout {
                                width: parent.width
                                Label {
                                    Layout.alignment: Qt.AlignLeft
                                    text: qsTr(section.name)
                                    font.pointSize: 14
                                }
                                Label {
                                    Layout.alignment: Qt.AlignRight
                                    color: "#b3ffffff"
                                    text: qsTr(section.detail)
                                    font.pointSize: 14
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
