import QtQuick 2.0
import Sailfish.Silica 1.0

// Edit the alt text of one attached image.
Dialog {
    id: dialog

    property string imagePath
    property string alt

    onAccepted: alt = altField.text

    Column {
        width: parent.width

        DialogHeader {
            acceptText: qsTr("Save")
            title: qsTr("Alt text")
        }

        Image {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            height: Math.min(width * (sourceSize.height / Math.max(sourceSize.width, 1)),
                             Screen.height / 3)
            source: dialog.imagePath
            asynchronous: true
            fillMode: Image.PreserveAspectFit
        }

        TextArea {
            id: altField
            width: parent.width
            text: dialog.alt
            placeholderText: qsTr("Describe the image")
            label: qsTr("Alt text")
            focus: true
        }
    }
}
