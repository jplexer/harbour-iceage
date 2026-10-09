import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0

Page {
    id: page

    // [{ fullsize, alt }] for every image in the post; swipe between them
    property var images: []
    property int currentIndex: 0

    readonly property var _current: images.length > 0 ? images[pager.currentIndex] : null

    allowedOrientations: Orientation.All

    MediaDownloader {
        id: downloader
        onFinished: Notices.show(qsTr("Saved to Gallery"))
        onDownloadError: Notices.show(qsTr("Download failed: %1").arg(message))
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: height

        PullDownMenu {
            busy: downloader.busy
            MenuItem {
                text: qsTr("Save to Gallery")
                enabled: !downloader.busy && page._current !== null
                onClicked: downloader.downloadImage(page._current.fullsize)
            }
        }

        // Same pattern as the profile pager: the horizontal list takes the
        // swipe, so overscrolling the first image navigates back by hand
        ListView {
            id: pager
            anchors.fill: parent
            orientation: ListView.Horizontal
            snapMode: ListView.SnapOneItem
            highlightRangeMode: ListView.StrictlyEnforceRange
            highlightMoveDuration: 0
            boundsBehavior: Flickable.DragOverBounds
            interactive: page.images.length > 1
            clip: true
            model: page.images
            currentIndex: page.currentIndex

            onDraggingChanged: {
                if (!dragging && currentIndex === 0
                        && contentX < originX - Theme.itemSizeSmall / 2)
                    pageStack.pop()
            }

            delegate: Item {
                width: pager.width
                height: pager.height

                Image {
                    id: image
                    anchors.fill: parent
                    source: modelData.fullsize
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    size: BusyIndicatorSize.Large
                    running: image.status === Image.Loading
                }
            }
        }
    }

    Label {
        visible: page.images.length > 1
        anchors {
            top: parent.top
            topMargin: Theme.paddingLarge
            horizontalCenter: parent.horizontalCenter
        }
        text: qsTr("%1 / %2").arg(pager.currentIndex + 1).arg(page.images.length)
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.secondaryColor
    }

    Label {
        readonly property string altText: page._current && page._current.alt
                                          ? page._current.alt : ""
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
