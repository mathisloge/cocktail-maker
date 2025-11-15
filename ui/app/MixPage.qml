// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Page {
    id: root
    signal cancelClicked
    signal finished
    required property RecipeDetail recipe

    header: Item {
        implicitHeight: headerLabel.height

        Button {
            width: 150
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            colorStart: "#ef4444"
            colorEnd: "#dc2626"
            text: qsTr("Abbrechen")
            onClicked: root.cancelClicked()
        }

        Label {
            id: headerLabel
            text: qsTr("%1 wird zubereitet").arg(root.recipe.name)
            wrapMode: Text.Wrap
            font.pointSize: 48
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
        }

        Button {
            width: 150
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            colorStart: "#ef4444"
            colorEnd: "#dc2626"
            text: qsTr("Abbrechen")
            onClicked: root.cancelClicked()
        }
    }

    Section {
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width / 2
        ColumnLayout {
            width: parent.width
            Label {
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
    ManualCommandPopup {
        id: manualActionPopup
        anchors.centerIn: parent
        titleText: qsTr("Bitte führe den angezeigten Schritt aus:")
        instructionText: "..."
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
