// SPDX-FileCopyrightText: 2025 Mathis Logemann <mathis@quite.rocks>
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic

BusyIndicator {
    id: control

    contentItem: Item {
        implicitWidth: 40
        implicitHeight: 32

        Row {
            anchors.centerIn: parent
            spacing: 6
            opacity: control.running ? 1 : 0

            Behavior on opacity {
                OpacityAnimator {
                    duration: 250
                }
            }

            Repeater {
                model: 3

                Rectangle {
                    id: dots
                    width: 8
                    height: 8
                    radius: 4

                    required property int index
                    property int dotIndex: index
                    property var colors: ["#06b6d4", "#8b5cf6", "#ec4899"]
                    property int colorIndex: 0

                    color: colors[colorIndex]

                    Behavior on color {
                        ColorAnimation {
                            duration: 300
                            easing.type: Easing.InOutQuad
                        }
                    }

                    SequentialAnimation on y {
                        running: control.visible && control.running
                        loops: Animation.Infinite

                        PauseAnimation {
                            duration: dots.dotIndex * 150
                        }

                        NumberAnimation {
                            from: 0
                            to: -12
                            duration: 300
                            easing.type: Easing.InOutQuad
                        }

                        NumberAnimation {
                            from: -12
                            to: 0
                            duration: 300
                            easing.type: Easing.InOutQuad
                        }

                        PauseAnimation {
                            duration: (2 - dots.dotIndex) * 150
                        }
                    }

                    SequentialAnimation on colorIndex {
                        running: control.visible && control.running
                        loops: Animation.Infinite

                        PauseAnimation {
                            duration: dots.dotIndex * 150
                        }

                        ScriptAction {
                            script: colorIndex = 1
                        }

                        PauseAnimation {
                            duration: 300
                        }

                        ScriptAction {
                            script: colorIndex = 2
                        }

                        PauseAnimation {
                            duration: 300
                        }

                        ScriptAction {
                            script: colorIndex = 0
                        }

                        PauseAnimation {
                            duration: 300 + (2 - dots.dotIndex) * 150
                        }
                    }
                }
            }
        }
    }
}
