import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Item {
    id: root

    RowLayout {
        anchors.fill: parent
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: 200
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 50
            }
        }

        Loader {
            Layout.fillHeight: true
            Layout.fillWidth: true

            sourceComponent: weightSensorPage
        }
    }

    Component {
        id: weightSensorPage
        SettingsWeightSubPage {
        }
    }
}
