// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Effects

AbstractButton {
    id: root
    clip: true

    property string iconSrc: ""

    background: Rectangle {
        id: backgroundRoot
        color: "#09ffffff"
        border.color: "#0dffffff"
        border.width: 1
        radius: 16
        anchors.fill: parent

        Image {
            id: myIcon
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
            source: root.iconSrc
            sourceSize: Qt.size(width, height)
            visible: false
        }
        MultiEffect {
            source: myIcon
            anchors.fill: myIcon
            maskEnabled: true
            maskSource: mask
        }
        Rectangle {
            id: mask
            anchors.fill: parent
            radius: 24
            layer.enabled: true
            layer.smooth: true
            opacity: 0
            color: "#313131"
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation {
                    duration: 150
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: root.verticalCenter
        height: text.height + 20
        color: "#71313131"
    }

    Label {
        id: text
        anchors.centerIn: parent
        text: root.text
        font {
            pixelSize: 70
            weight: 725
        }
    }

    states: [
        State {
            name: "pressed"
            when: root.pressed

            PropertyChanges {
                mask.opacity: 0.7
            }
        }
    ]
}
