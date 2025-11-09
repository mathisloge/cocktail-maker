// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

Frame {
    id: root
    property color backgroundColor: "#09ffffff"
    background: Rectangle {
        color: root.backgroundColor
        border.color: "#0dffffff"
        border.width: 1
        radius: 16
    }
    padding: 20
}
