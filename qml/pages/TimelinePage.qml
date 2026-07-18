import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"

Page {
    id: page

    property Session session
    property TimelineModel timeline
    property NotificationsModel notifications

    Connections {
        target: session
        onLoggedInChanged: {
            if (session.loggedIn) {
                timeline.refresh()
            }
        }
    }

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: timeline

        header: PageHeader {
            title: "Ice Age"
            description: session.loggedIn ? "@" + session.handle : ""
        }

        PullDownMenu {
            busy: session.busy || timeline.loading
            MenuItem {
                text: qsTr("Sign in")
                visible: !session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("LoginPage.qml"), { session: session })
            }
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"), { session: session })
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
                text: notifications.unreadCount > 0
                      ? qsTr("Notifications (%1)").arg(notifications.unreadCount)
                      : qsTr("Notifications")
                visible: session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("NotificationsPage.qml"), {
                    session: session,
                    notifications: notifications
                })
            }
            MenuItem {
                text: qsTr("New post")
                visible: session.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("ComposePage.qml"), { session: session })
            }
            MenuItem {
                text: qsTr("Refresh")
                visible: session.loggedIn
                onClicked: timeline.refresh()
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
