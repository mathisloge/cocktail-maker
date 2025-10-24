// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Item {
    id: root
    signal cancelClicked
    signal finished
    required property RecipeDetail recipe
    Section {
        id: mixingSection
        anchors.centerIn: parent
        ColumnLayout {
            anchors.centerIn: parent
            Label {
                text: qsTr("%1 wird zubereitet").arg(root.recipe.name)
                font.pointSize: 48
                Layout.margins: 20
                Layout.alignment: Qt.AlignHCenter
            }

            Section {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                ColumnLayout {
                    width: parent.width
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        font.pointSize: 18
                        text: qsTr("Schritte:")
                    }

                    Repeater {
                        model: ApplicationState.recipeExecutor.commandStatusModel

                        delegate: Section {
                            id: section

                            required property string name
                            required property var status
                            Layout.fillWidth: true
                            RowLayout {
                                width: parent.width
                                Label {
                                    Layout.alignment: Qt.AlignLeft
                                    text: qsTr(section.name)
                                    font.pointSize: 14
                                }
                                BusyIndicator {
                                    Layout.alignment: Qt.AlignRight
                                    running: section.status === RecipeCommandStatusModel.CommandStatus.Started
                                }
                            }
                            states: [
                                State {
                                    name: "notStarted"
                                    when: section.status === RecipeCommandStatusModel.CommandStatus.NotStarted
                                },
                                State {
                                    name: "active"
                                    when: section.status === RecipeCommandStatusModel.CommandStatus.Started
                                    PropertyChanges {
                                        section.backgroundColor: "#65f59f0b"
                                    }
                                },
                                State {
                                    name: "finished"
                                    when: section.status === RecipeCommandStatusModel.CommandStatus.Finished
                                    PropertyChanges {
                                        section.backgroundColor: "#6510b981"
                                    }
                                }
                            ]
                            transitions: [
                                Transition {
                                    to: "*"
                                    ColorAnimation {
                                        target: section
                                        duration: 100
                                    }
                                }
                            ]
                        }
                    }
                }
            }

            ProgressBar {
                Layout.margins: 20
                Layout.fillWidth: true
                indeterminate: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Button {
                    colorStart: "#ef4444"
                    colorEnd: "#dc2626"
                    text: qsTr("Abbrechen")
                    onClicked: root.cancelClicked()
                }
            }
        }
    }
    ManualCommandPopup {
        id: manualActionPopup
        anchors.centerIn: parent
        titleText: qsTr("Bitte führe den angezeigten Schritt aus:")
        instructionText: "TBD"
        confirmButtonText: qsTr("Fertig ▶")

        onConfirmClicked: {
            manualActionPopup.close();
            ApplicationState.recipeExecutor.continue_mix();
        }
    }

    ManualCommandPopup {
        id: refillActionPopup
        anchors.centerIn: parent
        titleText: qsTr("Bitte fülle nach:")
        instructionText: "TBD"
        confirmButtonText: qsTr("Aufgefüllt ▶")

        onConfirmClicked: {
            refillActionPopup.close();
            ApplicationState.recipeExecutor.continue_mix();
        }
    }

    Connections {
        target: ApplicationState.recipeExecutor
        function onFinished() {
            root.finished();
        }

        function onManualActionRequired(instruction) {
            manualActionPopup.instructionText = qsTr(instruction);
            manualActionPopup.open();
        }

        function onRefillActionRequired(ingredient) {
            refillActionPopup.instructionText = qsTr(ingredient);
            refillActionPopup.open();
        }
    }
}
