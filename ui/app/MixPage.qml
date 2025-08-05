import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

Item {
    id: root
    signal cancelClicked
    signal finished
    required property RecipeDetail recipe
    Section {
        id: mixingSection
        anchors.centerIn: parent
        ColumnLayout {
            anchors.centerIn: parent
            Label {
                text: root.recipe.name + " wird zubereitet"
                font.pointSize: 48
                Layout.margins: 20
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                id: textLabel
                text: "Schritt 1/3"
                color: "#b3ffffff"
                font.pointSize: 28
                Layout.alignment: Qt.AlignHCenter
            }

            ProgressBar {
                Layout.margins: 20
                Layout.fillWidth: true
                indeterminate: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Button {
                    colorStart: "#ef4444"
                    colorEnd: "#dc2626"
                    text: "Abbrechen"
                    onClicked: root.cancelClicked()
                }
                Button {
                    text: "Getränk aufgefüllt ▶"
                    onClicked: ApplicationState.recipeExecutor.continue_mix()
                }
            }
        }
    }

    Connections {
        target: ApplicationState.recipeExecutor
        function finished() {
            textLabel.text = "FINISHED";
            root.finished();
        }
    }
}
