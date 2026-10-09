import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"
import "../components/linkutils.js" as LinkUtils

Page {
    id: page

    property Session session
    property string actor

    function openLink(link) {
        var parsed = LinkUtils.parseLink(link)
        if (parsed.type === "profile") {
            pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), {
                session: page.session,
                actor: parsed.value
            })
        } else if (parsed.type === "hashtag") {
            pageStack.push(Qt.resolvedUrl("SearchPage.qml"), {
                session: page.session,
                initialQuery: "#" + parsed.value
            })
        } else {
            Qt.openUrlExternally(link)
        }
    }

    // The header is glued to the top of the feed content: it scrolls up
    // with the top post until only the tab row remains, and rides back
    // down the same way. The panes reserve its space with a spacer, so
    // the header body never overlaps posts and needs no backdrop; only
    // the sticky tab row gets a panel gradient for posts passing under.
    // Keep the pinned tab row clear of a display cutout (portrait only,
    // like Silica's PageHeader)
    readonly property real _cutout: orientation === Orientation.Portrait
                                    ? Screen.topCutout.height : 0
    readonly property real _collapseMax: headerArea.height - tabRow.height - _cutout
    readonly property real _headerOffset: pager.currentItem
        ? Math.min(pager.currentItem.contentY + headerArea.height, _collapseMax)
        : 0

    AuthorFeedModel {
        id: feedPosts
        session: page.session
        actor: page.actor
    }

    AuthorFeedModel {
        id: feedReplies
        property bool started: false
        session: page.session
        actor: page.actor
        filter: "posts_with_replies"
    }

    AuthorFeedModel {
        id: feedMedia
        property bool started: false
        session: page.session
        actor: page.actor
        filter: "posts_with_media"
    }

    // getActorLikes only works for the signed-in account
    AuthorFeedModel {
        id: feedLikes
        property bool started: false
        session: page.session
        actor: page.actor
        filter: "likes"
    }

    readonly property bool _ownProfile: session.loggedIn
                                        && (actor === session.did
                                            || actor === session.handle
                                            || feedPosts.profile.did === session.did)
    readonly property var _panes: _ownProfile
                                  ? [feedPosts, feedReplies, feedMedia, feedLikes]
                                  : [feedPosts, feedReplies, feedMedia]
    readonly property var _tabTitles: _ownProfile
                                      ? [qsTr("Posts"), qsTr("Replies"), qsTr("Media"), qsTr("Likes")]
                                      : [qsTr("Posts"), qsTr("Replies"), qsTr("Media")]

    // The other tabs fetch the first time they're shown
    function _startPane(index) {
        var feed = _panes[index]
        if (feed && feed !== feedPosts && !feed.started) {
            feed.started = true
            feed.refresh()
        }
    }

    Component.onCompleted: feedPosts.refresh()

    Item {
        id: headerArea
        y: -page._headerOffset
        width: parent.width
        height: headerColumn.height

        Column {
            id: headerColumn
            width: parent.width

            // Everything above the tab row fades out as the header pins,
            // so no text lingers in the display cutout strip
            Column {
                id: bodyColumn
                width: parent.width
                opacity: page._collapseMax > 0
                         ? Math.max(0, Math.min(1,
                               (page._collapseMax - page._headerOffset)
                               / Math.max(page._cutout, Theme.itemSizeSmall)))
                         : 1

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
                    id: bioLabel

                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    visible: text.length > 0
                    textFormat: Text.StyledText
                    text: LinkUtils.linkify(feedPosts.profile.description !== undefined
                                            ? feedPosts.profile.description : "")
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.primaryColor
                    linkColor: Theme.highlightColor
                    // No MouseArea here: it would swallow drags (the header
                    // has no flickable ancestor to steal them), leaving a
                    // dead spot for scroll/pull-down over the bio
                    onLinkActivated: page.openLink(link)
                }

                Item {
                    width: parent.width
                    height: Theme.paddingMedium
                }

            }
            Item {
                id: tabRow
                width: parent.width
                height: Theme.itemSizeSmall

                // Posts scroll under the sticky tab row; panel-style
                // gradient. As the row pins, the backdrop grows upward to
                // also cover the display cutout strip above it.
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: page._collapseMax > 0
                            ? -page._cutout * Math.max(0, Math.min(1,
                                  page._headerOffset / page._collapseMax))
                            : 0
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Theme.rgba(Theme.highlightBackgroundColor, 0.5)
                        }
                        GradientStop {
                            position: 1.0
                            color: Theme.rgba(Theme.highlightBackgroundColor, 0.7)
                        }
                    }
                }

                Row {
                    anchors.fill: parent

                    Repeater {
                        model: page._tabTitles

                        BackgroundItem {
                            width: parent.width / page._tabTitles.length
                            height: parent.height
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
        }
    }

    // Horizontal, non-circular pager: swipe between Posts and
    // the other tabs, rubber-banding at the right end.
    ListView {
        id: pager
        anchors.fill: parent
        z: -1
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 300
        boundsBehavior: Flickable.DragOverBounds
        clip: true
        model: page._panes
        onCurrentIndexChanged: page._startPane(currentIndex)

        // Swiping right past the first pane navigates back
        onDraggingChanged: {
            if (!dragging && currentIndex === 0
                    && contentX < originX - Theme.itemSizeSmall / 2)
                pageStack.pop()
        }

        delegate: ProfileFeedPane {
            width: pager.width
            height: pager.height
            feed: modelData
            headerHeight: headerArea.height
        }
    }
}
