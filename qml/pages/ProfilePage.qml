import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"

Page {
    id: page

    property Session session
    property string actor

    AuthorFeedModel {
        id: feedPosts
        session: page.session
        actor: page.actor
    }

    AuthorFeedModel {
        id: feedReplies
        session: page.session
        actor: page.actor
        filter: "posts_with_replies"
    }

    Component.onCompleted: {
        feedPosts.refresh()
        feedReplies.refresh()
    }

    // Fixed profile header; only the feed area below swipes between tabs
    Column {
        id: headerColumn
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }

        Image {
            width: parent.width
            height: width / 3
            source: feedPosts.profile.banner !== undefined ? feedPosts.profile.banner : ""
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
            source: feedPosts.profile.avatar !== undefined ? feedPosts.profile.avatar : ""
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            truncationMode: TruncationMode.Fade
            text: feedPosts.profile.displayName !== undefined
                  ? feedPosts.profile.displayName : ""
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            truncationMode: TruncationMode.Fade
            text: {
                var handle = feedPosts.profile.handle !== undefined
                             ? "@" + feedPosts.profile.handle : ""
                if (feedPosts.profile.followsYou === true)
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
            maximumLineCount: 4
            elide: Text.ElideRight
            visible: text.length > 0
            text: feedPosts.profile.description !== undefined
                  ? feedPosts.profile.description : ""
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.primaryColor
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * Theme.horizontalPageMargin
            visible: feedPosts.profile.handle !== undefined
            text: qsTr("%1 posts · %2 followers · %3 following")
                  .arg(feedPosts.profile.postsCount !== undefined
                       ? feedPosts.profile.postsCount : 0)
                  .arg(feedPosts.profile.followersCount !== undefined
                       ? feedPosts.profile.followersCount : 0)
                  .arg(feedPosts.profile.followsCount !== undefined
                       ? feedPosts.profile.followsCount : 0)
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
                    onClicked: pager.currentIndex = index

                    Label {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: Theme.fontSizeSmall
                        color: pager.currentIndex === index
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
                        visible: pager.currentIndex === index
                    }
                }
            }
        }
    }

    // Horizontal, non-circular pager: swipe between Posts and
    // Posts & replies, rubber-banding at the ends.
    ListView {
        id: pager
        anchors {
            top: headerColumn.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 300
        boundsBehavior: Flickable.DragOverBounds
        clip: true
        model: [feedPosts, feedReplies]

        delegate: ProfileFeedPane {
            width: pager.width
            height: pager.height
            feed: modelData
        }
    }
}
