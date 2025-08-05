pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Controls
import QtQuick.Layouts
import CocktailMaker.Ui

ApplicationWindow {
    id: window
    visible: true
    width: 800
    height: 600
    title: "Cocktail Automat"

    SystemPalette {
        id: systemPalette
    }

    Frame {
        anchors.fill: parent
        background: Rectangle {
            color: "#1affffff"
            border.color: "#0dffffff"

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: "#581c87"
                }  // purple-900
                GradientStop {
                    position: 0.8
                    color: "#1e40af"
                }  // blue-800
                GradientStop {
                    position: 1.0
                    color: "#312e81"
                }  // indigo-800
            }
        }

        StackView {
            id: stackView
            anchors.fill: parent
            initialItem: cocktailSelectionPage
        }
    }

    Component {
        id: cocktailSelectionPage
        RecipeListPage {

        }
    }

    Component {
        id: cocktailDetailPage

        Item {
            id: cocktailDetailPageRoot
            readonly property int sectionWidth: 400
            ColumnLayout {
                width: parent.width
                spacing: 20

                Item {
                    Layout.preferredHeight: headerText.height
                    Layout.fillWidth: true
                    Button {
                        text: "Zurück"
                        onClicked: stackView.pop()
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Label {
                        id: headerText
                        text: "Piña Colada"
                        font.pointSize: 48
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
                Label {
                    text: "Tropical paradise in a glass"
                    font.pointSize: 20
                    color: "#b3ffffff"
                    Layout.alignment: Qt.AlignHCenter
                }

                Section {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: cocktailDetailPageRoot.sectionWidth

                    ColumnLayout {
                        width: parent.width
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            font.pointSize: 18
                            text: "Getränk boosten"
                        }

                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            font.pointSize: 18
                            text: boostSlider.value.toFixed(0) + "%"
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignHCenter
                            Label {
                                text: "mild"
                            }
                            Slider {
                                id: boostSlider
                                from: -50
                                value: 0
                                to: 50
                                stepSize: 5
                                snapMode: Slider.SnapOnRelease
                            }
                            Label {
                                text: "stark"
                            }
                        }
                    }
                }

                Section {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: cocktailDetailPageRoot.sectionWidth
                    ColumnLayout {
                        width: parent.width
                        Label {
                            Layout.alignment: Qt.AlignHCenter
                            font.pointSize: 18
                            text: "Zutaten:"
                        }

                        Section {
                            Layout.fillWidth: true
                            RowLayout {
                                width: parent.width
                                Label {
                                    Layout.alignment: Qt.AlignLeft
                                    text: "Weißer Rum"
                                    font.pointSize: 14
                                }
                                Label {
                                    Layout.alignment: Qt.AlignRight
                                    color: "#b3ffffff"
                                    text: "90 ml"
                                    font.pointSize: 14
                                }
                            }
                        }
                        Section {
                            Layout.fillWidth: true
                            RowLayout {
                                width: parent.width
                                Label {
                                    Layout.alignment: Qt.AlignLeft
                                    text: "Soda Wasser"
                                    font.pointSize: 14
                                }
                                Label {
                                    Layout.alignment: Qt.AlignRight
                                    color: "#b3ffffff"
                                    text: "240 ml"
                                    font.pointSize: 14
                                }
                            }
                        }
                    }
                }

                Button {
                    Layout.alignment: Qt.AlignHCenter
                    text: "🍹 mixen!"
                    onClicked: {
                        stackView.push(mixingPage);
                    }
                }
            }
        }
    }

    Component {
        id: mixingPage

        Item {
            Section {
                id: mixingSection
                anchors.centerIn: parent
                ColumnLayout {
                    anchors.centerIn: parent
                    Label {
                        text: "Piña Colada wird zubereitet"
                        font.pointSize: 48
                        Layout.margins: 20
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Label {
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
                            onClicked: stackView.pop()
                        }
                        Button {
                            text: "Getränk aufgefüllt ▶"
                        }
                    }
                }
            }
        }
    }
}
