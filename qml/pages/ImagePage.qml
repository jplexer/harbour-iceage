import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    property alias source: image.source
    property string altText

    allowedOrientations: Orientation.All

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        asynchronous: true
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: image.status === Image.Loading
    }

    Label {
        visible: altText.length > 0
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            margins: Theme.horizontalPageMargin
        }
        wrapMode: Text.Wrap
        maximumLineCount: 4
        truncationMode: TruncationMode.Elide
        text: altText
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }
}
