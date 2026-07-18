import QtQuick 2.0
import Sailfish.Silica 1.0

// One pane of the profile pager: the profile header plus one filtered
// author feed. The Posts / Posts & replies tab row doubles as the pager
// indicator; swiping the pager or tapping a tab switches panes.
SilicaListView {
    id: listView

    property var feed
    property int currentTab
    signal tabRequested(int tab)

    model: feed

    PullDownMenu {
        busy: feed.loading
        MenuItem {
            visible: feed.session.loggedIn
                     && feed.profile.did !== undefined
                     && feed.profile.did !== feed.session.did
            enabled: feed.profile.followingUri !== "pending"
            text: feed.profile.followingUri !== undefined
                  && feed.profile.followingUri.length > 0
                  ? qsTr("Unfollow") : qsTr("Follow")
            onClicked: feed.toggleFollow()
        }
        MenuItem {
            text: qsTr("Refresh")
            onClicked: feed.refresh()
        }
    }

    header: Column {
        width: listView.width

        Image {
            width: parent.width
            height: width / 3
            source: feed.profile.banner !== undefined ? feed.profile.banner : ""
            visible: source != ""
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
        }

        Item {
            width: parent.width
            height: Theme.paddingLarge
        }

        Image {
            x: Theme.horizontalPageMargin
            width: Theme.itemSizeLarge
            height: Theme.itemSizeLarge
            source: feed.profile.avatar !== undefined ? feed.profile.avatar : ""
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            truncationMode: TruncationMode.Fade
            text: feed.profile.displayName !== undefined ? feed.profile.displayName : ""
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            truncationMode: TruncationMode.Fade
            text: {
                var handle = feed.profile.handle !== undefined
                             ? "@" + feed.profile.handle : ""
                if (feed.profile.followsYou === true)
                    handle += " · " + qsTr("Follows you")
                return handle
            }
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.secondaryColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            wrapMode: Text.Wrap
            visible: text.length > 0
            text: feed.profile.description !== undefined ? feed.profile.description : ""
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: feed.profile.handle !== undefined
            text: qsTr("%1 posts · %2 followers · %3 following")
                  .arg(feed.profile.postsCount !== undefined ? feed.profile.postsCount : 0)
                  .arg(feed.profile.followersCount !== undefined ? feed.profile.followersCount : 0)
                  .arg(feed.profile.followsCount !== undefined ? feed.profile.followsCount : 0)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }

        Item {
            width: parent.width
            height: Theme.paddingMedium
        }

        Row {
            width: parent.width

            Repeater {
                model: [qsTr("Posts"), qsTr("Posts & replies")]

                BackgroundItem {
                    width: parent.width / 2
                    height: Theme.itemSizeSmall
                    onClicked: listView.tabRequested(index)

                    Label {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: Theme.fontSizeSmall
                        color: listView.currentTab === index
                               ? Theme.highlightColor : Theme.secondaryColor
                    }

                    Rectangle {
                        anchors {
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                        }
                        height: Math.round(Theme.pixelRatio * 2)
                        color: Theme.highlightColor
                        visible: listView.currentTab === index
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: Theme.paddingMedium
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: feed.loading && listView.count === 0
    }

    delegate: PostDelegate {
        feedModel: listView.feed
    }

    onAtYEndChanged: {
        if (atYEnd && count > 0) {
            feed.loadMore()
        }
    }

    VerticalScrollDecorator { }
}
