import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0

// Pick which feed the timeline shows: Following or a pinned custom feed.
Page {
    id: page

    property Session session
    property TimelineModel timeline

    FeedListModel {
        id: feeds
        session: page.session
    }

    Component.onCompleted: feeds.refresh()

    function select(uri, name) {
        timeline.feedName = name
        timeline.feedUri = uri
        pageStack.pop()
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: feeds

        PullDownMenu {
            busy: feeds.loading
            MenuItem {
                text: qsTr("Refresh")
                onClicked: feeds.refresh()
            }
        }

        header: Column {
            width: listView.width

            PageHeader {
                title: qsTr("Feeds")
            }

            BackgroundItem {
                width: parent.width
                onClicked: page.select("", "")

                Label {
                    x: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Following")
                    color: timeline.feedUri === ""
                           ? Theme.highlightColor : Theme.primaryColor
                }
            }
        }

        delegate: BackgroundItem {
            id: feedItem
            width: listView.width

            onClicked: page.select(uri, name)

            Image {
                id: feedAvatar
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.iconSizeSmall
                height: Theme.iconSizeSmall
                source: avatar
                visible: avatar.length > 0
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
            }

            Label {
                anchors {
                    left: feedAvatar.visible ? feedAvatar.right : parent.left
                    leftMargin: feedAvatar.visible ? Theme.paddingMedium
                                                   : Theme.horizontalPageMargin
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                text: name
                color: timeline.feedUri === uri
                       ? Theme.highlightColor : Theme.primaryColor
            }
        }

        ViewPlaceholder {
            enabled: !feeds.loading && listView.count === 0
            text: qsTr("No pinned feeds")
            hintText: qsTr("Pin feeds in another Bluesky client to see them here")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: feeds.loading && listView.count === 0
        }

        VerticalScrollDecorator { }
    }
}
