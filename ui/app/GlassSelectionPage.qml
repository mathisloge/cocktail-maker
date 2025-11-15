// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Page {
    id: root

    required property RecipeDetail recipe
    property glass detectedGlass: glassDetector.detectedGlass

    signal backClicked
    signal glassSelected(id: string)

    property string _selectedGlassId: detectedGlass.id

    GlassListModel {
        id: model
        glassStore: ApplicationState.glassStore
    }

    GlassDetector {
        id: glassDetector
        glassStore: ApplicationState.glassStore
    }

    header: Item {
        implicitHeight: headerLabel.height
        Button {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 150
            text: qsTr("Zurück")
            onClicked: root.backClicked()
        }

        Label {
            id: headerLabel
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Glas wählen")
            font.pointSize: 48
        }
    }

    GridView {
        id: glassGrid
        readonly property int spacing: 10
        anchors.fill: parent
        anchors.margins: 50
        cellWidth: width / 2
        cellHeight: 120
        currentIndex: 0 // default active
        clip: true
        //highlight: highlight
        model: model
        delegate: Button {
            width: glassGrid.cellWidth - glassGrid.spacing
            height: glassGrid.cellHeight - glassGrid.spacing
            required property string glassId
            required property string name
            required property string capacity
            required property int index

            text: name + ": " + capacity

            onClicked: {
                glassGrid.currentIndex = index;
                root._selectedGlassId = glassId;
                root.glassSelected(root._selectedGlassId);
            }
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
