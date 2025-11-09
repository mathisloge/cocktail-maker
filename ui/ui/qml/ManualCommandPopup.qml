// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Popup {
    id: root
    required property string titleText
    required property string instructionText
    required property string confirmButtonText
    signal confirmClicked
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose
    popupType: Popup.Item

    background: Rectangle {
        color: "#212121"
        border.color: '#7e7e7e'
        border.width: 2
        radius: 16
    }
    padding: 25

    Overlay.modal: Rectangle {
        color: "#1affffff"
        border.color: "#0dffffff"

        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: "#d0591c87"
            }  // purple-900
            GradientStop {
                position: 0.8
                color: "#d01e40af"
            }  // blue-800
            GradientStop {
                position: 1.0
                color: "#d0312e81"
            }  // indigo-800
        }
    }

    contentItem: ColumnLayout {

        Label {
            text: root.titleText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 20
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.width
        }

        Label {
            id: popupText
            text: root.instructionText
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pointSize: 28
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: root.width
        }
        Button {
            text: root.confirmButtonText
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            onClicked: {
                root.confirmClicked();
            }
        }
    }
}
