import QtQuick

Rectangle {
    id: scrollIndicator
    required property bool down
    required property bool atEnd
    required property bool movingVertically

    width: 50
    height: 50
    radius: 25
    //color: '#3f3f3f'
    gradient: Gradient {
        orientation: Gradient.Horizontal
        // green-400 to blue-500
        GradientStop {
            position: 0.0
            color: "#10b981"
        }
        GradientStop {
            position: 1.0
            color: "#3b82f6"
        }
    }

    opacity: (atEnd || movingVertically) ? 0 : 0.8
    visible: opacity > 0

    Behavior on opacity {
        SequentialAnimation {
            PauseAnimation {
                duration: scrollIndicator.opacity > 0 ? 0 : 500
            }
            NumberAnimation {
                duration: 300
            }
        }
    }

    // Down arrow
    Text {
        anchors.centerIn: parent
        text: scrollIndicator.down ? "▼" : "▲"
        font.pixelSize: 24
        color: "white"
    }

    SequentialAnimation on anchors.bottomMargin {
        running: scrollIndicator.down && scrollIndicator.visible
        loops: Animation.Infinite
        NumberAnimation {
            to: 20
            duration: 1000
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            to: 10
            duration: 1000
            easing.type: Easing.InOutQuad
        }
    }

    SequentialAnimation on anchors.topMargin {
        running: !scrollIndicator.down && scrollIndicator.visible
        loops: Animation.Infinite
        NumberAnimation {
            to: 20
            duration: 1000
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            to: 10
            duration: 1000
            easing.type: Easing.InOutQuad
        }
    }
}
