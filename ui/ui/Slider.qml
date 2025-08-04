import QtQuick
import QtQuick.Controls.Basic

Slider {
    id: control
    value: 0.5

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 250
        implicitHeight: 16
        width: control.availableWidth
        height: implicitHeight
        radius: 8
        color: "#33ffffff"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            color: "#10b981"
            radius: 8
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 32
        implicitHeight: 32
        radius: 16
        color: control.pressed ? "#f0f0f0" : "#f6f6f6"
        border.color: "#bdbebf"
    }
}
