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

    // Swipe left/right to move between Posts and Posts & replies
    SlideshowView {
        id: pager
        anchors.fill: parent
        clip: true
        model: [feedPosts, feedReplies]

        delegate: ProfileFeedPane {
            width: pager.itemWidth
            height: pager.itemHeight
            feed: modelData
            currentTab: pager.currentIndex
            onTabRequested: pager.currentIndex = tab
        }
    }
}
