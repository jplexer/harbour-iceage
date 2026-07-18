import QtQuick 2.0
import Sailfish.Silica 1.0

// One pane of the profile pager: a single filtered author feed. The
// profile header sits above the pager in ProfilePage and collapses
// based on this list's scroll position.
SilicaListView {
    id: listView

    property var feed

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
