// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control
    text: "Button"
    font.pointSize: 18

    property color colorStart: "#10b981"
    property color colorEnd: "#3b82f6"

    contentItem: Text {
        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: control.down ? "#dddddd" : "#ffffff"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 150
        implicitHeight: 50
        opacity: enabled ? 1 : 0.3
        radius: 12
        gradient: Gradient {
            orientation: Gradient.Horizontal
            // green-400 to blue-500
            GradientStop {
                position: 0.0
                color: control.down ? Qt.darker(control.colorStart) : control.colorStart
            }
            GradientStop {
                position: 1.0
                color: control.down ? Qt.darker(control.colorEnd) : control.colorEnd
            }
        }
    }
}
