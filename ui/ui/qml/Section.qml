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
}
