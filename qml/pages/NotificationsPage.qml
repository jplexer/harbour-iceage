import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0

Page {
    id: page

    property Session session
    property NotificationsModel notifications

    function reasonText(reason, name) {
        switch (reason) {
        case "like": return qsTr("%1 liked your post").arg(name)
        case "like-via-repost": return qsTr("%1 liked your repost").arg(name)
        case "repost": return qsTr("%1 reposted your post").arg(name)
        case "repost-via-repost": return qsTr("%1 reposted your repost").arg(name)
        case "follow": return qsTr("%1 followed you").arg(name)
        case "mention": return qsTr("%1 mentioned you").arg(name)
        case "reply": return qsTr("%1 replied to you").arg(name)
        case "quote": return qsTr("%1 quoted your post").arg(name)
        case "starterpack-joined": return qsTr("%1 joined via your starter pack").arg(name)
        default: return qsTr("%1 sent a notification").arg(name)
        }
    }

    function openProfile(handle) {
        pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), {
            session: page.session,
            actor: handle
        })
    }

    onStatusChanged: {
        if (status === PageStatus.Active) {
            notifications.refresh()
            notifications.markSeen()
        }
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: notifications

        header: PageHeader {
            title: qsTr("Notifications")
        }

        PullDownMenu {
            busy: notifications.loading
            MenuItem {
                text: qsTr("Refresh")
                onClicked: notifications.refresh()
            }
        }

        ViewPlaceholder {
            enabled: listView.count === 0 && !notifications.loading
            text: qsTr("No notifications")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: notifications.loading && listView.count === 0
        }

        delegate: ListItem {
            id: item
            width: listView.width
            contentHeight: row.height + 2 * Theme.paddingMedium

            onClicked: {
                // Threads can only show real post records; like/repost
                // notification uris point at the like/repost record itself,
                // so prefer reasonSubject and fall back to the profile.
                function isPost(u) {
                    return u.indexOf("/app.bsky.feed.post/") >= 0
                }
                if (reason === "follow") {
                    page.openProfile(authorHandle)
                } else if (isPost(reasonSubject)) {
                    pageStack.push(Qt.resolvedUrl("ThreadPage.qml"), {
                        session: page.session,
                        postUri: reasonSubject
                    })
                } else if (isPost(uri)) {
                    pageStack.push(Qt.resolvedUrl("ThreadPage.qml"), {
                        session: page.session,
                        postUri: uri
                    })
                } else {
                    page.openProfile(authorHandle)
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: !isRead
                color: Theme.rgba(Theme.highlightBackgroundColor,
                                  Theme.highlightBackgroundOpacity * 0.3)
            }

            Row {
                id: row
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    top: parent.top
                    topMargin: Theme.paddingMedium
                }
                spacing: Theme.paddingMedium

                Image {
                    width: Theme.itemSizeExtraSmall
                    height: Theme.itemSizeExtraSmall
                    source: authorAvatar
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop

                    MouseArea {
                        anchors.fill: parent
                        onClicked: page.openProfile(authorHandle)
                    }
                }

                Column {
                    width: parent.width - Theme.itemSizeExtraSmall - parent.spacing

                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        truncationMode: TruncationMode.Elide
                        text: page.reasonText(reason, authorName)
                        font.pixelSize: Theme.fontSizeSmall
                        color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    }

                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        text: Format.formatDate(indexedAt, Formatter.DurationElapsed)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }

                    Label {
                        visible: text.length > 0
                        width: parent.width
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        truncationMode: TruncationMode.Elide
                        text: model.text
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                    }
                }
            }

            Separator {
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    bottom: parent.bottom
                }
                color: Theme.rgba(Theme.secondaryColor, 0.4)
            }
        }

        onAtYEndChanged: {
            if (atYEnd && count > 0) {
                notifications.loadMore()
            }
        }

        VerticalScrollDecorator { }
    }
}
