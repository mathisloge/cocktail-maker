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

    property glass detectedGlass
    required property RecipeDetail recipe
    signal backClicked
    signal nextClicked

    GlassListModel {
        id: model
        glassStore: ApplicationState.glassStore
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 20

        Section {
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            Layout.preferredWidth: 600
            Layout.fillHeight: true

            ColumnLayout {
                id: mainLayout
                width: parent.width
                height: parent.height

                Label {
                    text: "Glasgöße wählen"
                    font.pointSize: 48
                    Layout.margins: 20
                    Layout.alignment: Qt.AlignHCenter
                }

                Label {
                    text: "Erkannt: " + root.detectedGlass.capacity
                    font.pointSize: 28
                    Layout.margins: 20
                    Layout.alignment: Qt.AlignHCenter
                }

                GridView {
                    id: glassGrid
                    readonly property int spacing: 10
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: cellHeight + (count > 2 ? spacing : 0)
                    Layout.preferredHeight: (((count + 1) / 2) * cellHeight) - spacing
                    cellWidth: width / 2
                    cellHeight: 120
                    currentIndex: 0 // default active
                    clip: true
                    highlight: highlight
                    model: model
                    delegate: Button {
                        width: glassGrid.cellWidth - glassGrid.spacing
                        height: glassGrid.cellHeight - glassGrid.spacing
                        required property string name
                        required property string capacity
                        required property int index

                        text: name + ": " + capacity

                        onClicked: {
                            glassGrid.currentIndex = index;
                        }
                    }
                }
            }
        }
        Button {
            Layout.alignment: Qt.AlignTop | Qt.AlignHCenter
            text: "🍹 mixen!"
            onClicked: root.nextClicked()
        }
    }

    Component {
        id: highlight
        Rectangle {
            width: glassGrid.cellWidth
            height: glassGrid.cellHeight
            color: "transparent"
            border.color: "#fff"
            border.width: 8
            radius: 12
            x: glassGrid.currentItem.x
            y: glassGrid.currentItem.y
            z: 10
            Behavior on x {
                BounceAnimation {}
            }
            Behavior on y {
                BounceAnimation {}
            }
        }
    }

    component BounceAnimation: NumberAnimation {
        duration: 200
        easing.type: Easing.InOutQuad
    }
}
