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

        Item {
            RowLayout {
                id: headerLayout
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                Text {
                    text: "🍹 Cocktail Automat"
                    font.family: "Helvetica"
                    font.pointSize: 48
                    color: systemPalette.text

                    Layout.alignment: Qt.AlignHCenter
                }
            }
            ScrollView {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: headerLayout.bottom

                ListModel {
                    id: cocktails
                    ListElement {
                        name: "Mojito"
                        description: "A refreshing Cuban cocktail"
                    }
                    ListElement {
                        name: "Margarita"
                        description: "Classic Mexican tequila cocktail"
                    }
                    ListElement {
                        name: "Old Fashioned"
                        description: "Timeless whiskey cocktail"
                    }
                    ListElement {
                        name: "Cosmopolitan"
                        description: "Elegant pink cocktail"
                    }
                    ListElement {
                        name: "Piña Colada"
                        description: "Tropical paradise in a glass"
                    }
                    ListElement {
                        name: "Manhattan"
                        description: "Sophisticated whiskey cocktail"
                    }
                }
                GridLayout {
                    id: cocktailGrid
                    columns: 3
                    columnSpacing: 20
                    rowSpacing: 20
                    width: window.width
                }

                Instantiator {
                    id: cocktailInstantiator
                    model: cocktails

                    delegate: ImageCard {
                        required property var model
                        clip: true
                        Layout.preferredWidth: (cocktailGrid.width - ((cocktailGrid.columns - 1) * cocktailGrid.columnSpacing)) / cocktailGrid.columns
                        Layout.preferredHeight: width
                        text: model.name
                        iconSrc: "mojito.png"

                        parent: cocktailGrid
                        onClicked: {
                            stackView.push(cocktailDetailPage);
                        }
                    }
                }
            }
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
