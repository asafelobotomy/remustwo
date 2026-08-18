import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    width: 720
    height: 480
    visible: true
    title: "remustwo"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: "remustwo library browser"
            font.pixelSize: 22
            font.bold: true
        }

        Label {
            text: "Catalog: " + libraryModel.catalogPath
            wrapMode: Text.Wrap
        }

        Label {
            text: "Library: " + libraryModel.libraryPath
            wrapMode: Text.Wrap
        }

        Button {
            text: "Refresh library count"
            onClicked: libraryModel.fileCount()
        }

        Label {
            id: statusLabel
            text: libraryModel.status
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        TextField {
            id: matchPath
            placeholderText: "Path to ROM file for catalog match"
            Layout.fillWidth: true
        }

        Button {
            text: "Match file"
            onClicked: {
                const title = libraryModel.matchFile(matchPath.text)
                statusLabel.text = title.length > 0 ? ("Match: " + title) : libraryModel.status
            }
        }
    }
}
