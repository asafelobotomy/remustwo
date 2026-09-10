import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 780
    height: 560
    visible: true
    title: "remustwo"

    FolderDialog {
        id: scanDialog
        title: "Scan ROM folder"
        onAccepted: libraryModel.scanFolder(selectedFolder)
    }

    FolderDialog {
        id: destDialog
        title: "Organize destination"
        onAccepted: libraryModel.organize(selectedFolder, dryRunBox.checked, bundleBox.checked, artBox.checked, onlineBox.checked, "auto")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        Label {
            text: "remustwo library"
            font.pixelSize: 22
            font.bold: true
        }

        Label {
            text: "Catalog: " + libraryModel.catalogPath
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Label {
            text: "Library: " + libraryModel.libraryPath
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "Scan folder"
                enabled: !libraryModel.busy
                onClicked: scanDialog.open()
            }
            Button {
                text: "Match library"
                enabled: !libraryModel.busy
                onClicked: libraryModel.matchAll()
            }
            Button {
                text: "Verify"
                enabled: !libraryModel.busy
                onClicked: libraryModel.verify()
            }
            Button {
                text: "Enrich"
                enabled: !libraryModel.busy
                onClicked: libraryModel.enrich(onlineBox.checked, dryRunBox.checked)
            }
            Button {
                text: "Organize"
                enabled: !libraryModel.busy
                onClicked: destDialog.open()
            }
            Button {
                text: "Refresh"
                enabled: !libraryModel.busy
                onClicked: libraryModel.fileCount()
            }
        }

        RowLayout {
            CheckBox {
                id: dryRunBox
                text: "Dry run"
                checked: true
            }
            CheckBox {
                id: bundleBox
                text: "Bundle"
                checked: true
            }
            CheckBox {
                id: artBox
                text: "Include art"
            }
            CheckBox {
                id: onlineBox
                text: "Online"
            }
        }

        Label {
            id: statusLabel
            text: libraryModel.status
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: matchPath
                placeholderText: "Path to ROM file for catalog match"
                Layout.fillWidth: true
                enabled: !libraryModel.busy
            }
            Button {
                text: "Match file"
                enabled: !libraryModel.busy
                onClicked: {
                    const title = libraryModel.matchFile(matchPath.text)
                    statusLabel.text = title.length > 0 ? ("Match: " + title) : libraryModel.status
                }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: libraryModel
            delegate: RowLayout {
                width: ListView.view.width
                spacing: 8
                Image {
                    width: 48
                    height: 48
                    fillMode: Image.PreserveAspectFit
                    source: libraryModel.coverForRow(index, true)
                    asynchronous: true
                }
                ColumnLayout {
                    Label { text: title; font.bold: true }
                    Label { text: path; wrapMode: Text.Wrap; Layout.fillWidth: true }
                }
            }
        }
    }
}
