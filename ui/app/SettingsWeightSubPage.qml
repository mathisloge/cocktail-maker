import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Section {

    ColumnLayout {

        Label {
            font.pointSize: 18
            text: "Measure: " + 100 + "g"
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: refWeightInput
                Layout.fillWidth: true
            }
            Button {
                text: "Calibrate"
            }
        }
        Button {
            text: "Tare"
        }
    }
}
