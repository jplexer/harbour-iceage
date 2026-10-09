import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"

Page {
    id: page

    property Session session
    property TimelineModel timeline
    property NotificationsModel notifications

    // Notifications live one swipe to the left. Pushing another page
    // drops the attached page, so re-attach whenever we're back on top.
    function attachNotifications() {
        if (session.loggedIn && status === PageStatus.Active && !pageStack.nextPage(page)) {
            pageStack.pushAttached(Qt.resolvedUrl("NotificationsPage.qml"), {
                session: session,
                notifications: notifications
            })
        }
    }

    onStatusChanged: attachNotifications()

    // Catch up when the app comes back to the foreground after a while
    property date _lastForeground: new Date()

    Connections {
        target: Qt.application
        onStateChanged: {
            if (Qt.application.state !== Qt.ApplicationActive || !session.loggedIn)
                return
            var now = new Date()
            if (now - page._lastForeground > 5 * 60 * 1000)
                timeline.loadNewer()
            page._lastForeground = now
        }
    }

    Connections {
        target: session
        onLoggedInChanged: {
            if (session.loggedIn) {
                timeline.refresh()
                page.attachNotifications()
            } else if (pageStack.nextPage(page)) {
                pageStack.popAttached(page, PageStackAction.Immediate)
            }
        }
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: timeline

        // Tap the header to switch feeds
        header: BackgroundItem {
            id: headerItem
            width: listView.width
            height: pageHeader.height
            enabled: session.loggedIn
            onClicked: pageStack.push(Qt.resolvedUrl("FeedsPage.qml"), {
                session: session,
                timeline: timeline
            })

            PageHeader {
                id: pageHeader
                // With an attached page, PageHeader adds its own mouse
                // area that navigates forward, stealing our tap
                interactive: false
                highlighted: headerItem.highlighted
                title: !session.loggedIn ? "Ice Age"
                       : timeline.feedUri.length > 0 ? timeline.feedName
                       : qsTr("Following")
                description: {
                    if (!session.loggedIn)
                        return ""
                    var text = "@" + session.handle
                    if (notifications.unreadCount > 0)
                        text += " · " + qsTr("%1 new").arg(notifications.unreadCount)
                    return text
                }
            }
        }

        PullDownMenu {
            busy: session.busy || timeline.loading
            MenuItem {
                text: qsTr("Sign in")
                visible: !session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"), { session: session })
            }
            MenuItem {
                text: qsTr("My account")
                visible: session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("ProfilePage.qml"), {
                    session: session,
                    actor: session.did
                })
            }
            MenuItem {
                text: qsTr("Search")
                visible: session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("SearchPage.qml"), { session: session })
            }
            MenuItem {
                text: qsTr("New post")
                visible: session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("ComposePage.qml"), { session: session })
            }
            MenuItem {
                text: qsTr("Refresh")
                visible: session.loggedIn
                onClicked: timeline.loadNewer()
            }
        }

        ViewPlaceholder {
            enabled: !session.loggedIn && !session.busy
            text: qsTr("Not signed in")
            hintText: qsTr("Pull down to sign in with an app password")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: (session.busy || timeline.loading) && listView.count === 0
        }

        delegate: PostDelegate {
            feedModel: timeline
        }

        footer: Item {
            width: listView.width
            height: moreIndicator.running ? Theme.itemSizeMedium : 0
            BusyIndicator {
                id: moreIndicator
                anchors.centerIn: parent
                size: BusyIndicatorSize.Medium
                running: timeline.loading && listView.count > 0
            }
        }

        onAtYEndChanged: {
            if (atYEnd && count > 0) {
                timeline.loadMore()
            }
        }

        VerticalScrollDecorator { }
    }
}
