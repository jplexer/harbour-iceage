import QtQuick 2.0
import Sailfish.Silica 1.0

// One pane of the profile pager: a single filtered author feed. The
// profile header floats above the pager in ProfilePage, glued to this
// list's scroll position; the spacer header reserves its space.
SilicaListView {
    id: listView

    property var feed
    property real headerHeight
    property real _prevHeaderHeight: 0

    // The header grows as banner/bio load; if the view was resting at
    // the top, stay glued to the top so the first pull-down still opens
    // the menu instead of eating the growth first.
    onHeaderHeightChanged: {
        if (!dragging && !flicking && contentY <= -_prevHeaderHeight + 1)
            contentY = -headerHeight
        _prevHeaderHeight = headerHeight
    }

    model: feed

    header: Item {
        width: 1
        height: listView.headerHeight
    }

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
            text: qsTr("Followers (%1)")
                  .arg(feed.profile.followersCount !== undefined
                       ? feed.profile.followersCount : 0)
            onClicked: pageStack.push(Qt.resolvedUrl("../pages/ActorListPage.qml"), {
                session: feed.session,
                actor: feed.actor,
                mode: "followers"
            })
        }
        MenuItem {
            text: qsTr("Following (%1)")
                  .arg(feed.profile.followsCount !== undefined
                       ? feed.profile.followsCount : 0)
            onClicked: pageStack.push(Qt.resolvedUrl("../pages/ActorListPage.qml"), {
                session: feed.session,
                actor: feed.actor,
                mode: "following"
            })
        }
        MenuItem {
            text: qsTr("Refresh")
            onClicked: feed.refresh()
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
