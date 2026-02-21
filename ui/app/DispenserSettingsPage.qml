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
    header: Item {
        implicitHeight: backButton.height
        Button {
            id: backButton
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 150
            text: qsTr("Zurück")
            onClicked: root.backClicked()
        }
    }

    LiquidDispenserListModel {
        id: dispenserModel
        executionContextAdapter: ApplicationState.executionContextAdapter
    }

    Section {
        anchors.fill: parent
        anchors.margins: 50
        ColumnLayout {
            anchors.fill: parent
            Label {
                text: "Dispenser calibration"
                font.pointSize: 24
            }

            ListView {
                id: view
                model: dispenserModel
                clip: true
                spacing: 5
                Layout.fillWidth: true
                Layout.fillHeight: true
                delegate: Section {
                    id: stepperRoot
                    property bool needsRun: true
                    property int steps

                    required property string name
                    readonly property LiquidDispenserModel dispenser: ApplicationState.executionContextAdapter.create_dispenser_model(name)

                    width: ListView.view.width
                    height: 100

                    RowLayout {
                        id: layout
                        width: parent.width
                        visible: stepperRoot.dispenser

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            text: stepperRoot.name
                            font.pointSize: 14
                        }

                        TextField {
                            Layout.fillHeight: true
                            Layout.minimumWidth: 150
                            placeholderText: "steps"
                            font.pointSize: 14
                            validator: IntValidator {
                                bottom: 500
                                top: 10000
                            }

                            onTextChanged: {
                                if (text !== "") {
                                    stepperRoot.steps = parseInt(text);
                                }
                            }
                        }

                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 150
                            text: "Run"
                            visible: !stepperRoot.dispenser.running

                            onClicked: {
                                stepperRoot.dispenser.run(stepperRoot.steps);
                            }
                        }

                        Button {
                            Layout.alignment: Qt.AlignVCenter
                            Layout.preferredWidth: 150
                            colorStart: "#ef4444"
                            colorEnd: "#dc2626"
                            text: qsTr("Abbrechen")
                            visible: stepperRoot.dispenser.running
                            onClicked: {
                                stepperRoot.dispenser.stop();
                            }
                        }

                        TextField {
                            Layout.fillHeight: true
                            Layout.minimumWidth: 150
                            enabled: !stepperRoot.dispenser.running
                            font.pointSize: 14
                            placeholderText: "Measured ml"
                            validator: IntValidator {
                                bottom: 1
                                top: 1000
                            }

                            onTextChanged: {
                                if (text !== "") {
                                    stepperRoot.dispenser.update_pumped(parseInt(text));
                                }
                            }
                        }

                        BusyIndicator {
                            visible: stepperRoot.dispenser.running
                        }

                        Label {
                            Layout.alignment: Qt.AlignVCenter
                            font.pointSize: 24
                            text: stepperRoot.dispenser?.steps_per_litre
                            visible: !stepperRoot.dispenser.running
                        }
                    }
                }
            }
        }
    }
}
