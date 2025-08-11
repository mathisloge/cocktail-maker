pragma ComponentBehavior: Bound
// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Item {
    id: root

    required property RecipeDetail recipe
    signal backClicked
    signal nextClicked

    ColumnLayout {
        width: parent.width
        spacing: 20

        Section {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 600
            ColumnLayout {
                width: parent.width

                GridView {
                    id: glassGrid
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min((count / 2) * cellHeight, 600)
                    Layout.alignment: Qt.AlignHCenter
                    cellWidth: width / 2
                    cellHeight: 120

                    clip: true

                    model: ListModel {
                        ListElement {
                            name: "Bill Smith"
                            capacity: "40 ml"
                        }
                        ListElement {
                            name: "Bill Smith"
                            capacity: "40 ml"
                        }
                        ListElement {
                            name: "Bill Smith"
                            capacity: "40 ml"
                        }

                        ListElement {
                            name: "Bill Smith"
                            capacity: "40 ml"
                        }
                    }
                    delegate: Button {
                        width: glassGrid.cellWidth - 8
                        height: glassGrid.cellHeight - 8
                        required property string name
                        required property string capacity
                        text: name + ": " + capacity
                    }
                }
            }
        }
        Button {
            Layout.alignment: Qt.AlignHCenter
            text: "🍹 mixen!"
            onClicked: root.nextClicked()
        }
    }
}
