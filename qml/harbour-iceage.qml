import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import harbour.iceage 1.0
import "pages"

ApplicationWindow {
    id: app

    Session {
        id: blueskySession
        Component.onCompleted: resume()
    }

    TimelineModel {
        id: timelineModel
        session: blueskySession
    }

    NotificationsModel {
        id: notificationsModel
        session: blueskySession

        onAlert: {
            var summary
            switch (reason) {
            case "like": summary = qsTr("%1 liked your post").arg(authorName); break
            case "like-via-repost": summary = qsTr("%1 liked your repost").arg(authorName); break
            case "repost": summary = qsTr("%1 reposted your post").arg(authorName); break
            case "repost-via-repost": summary = qsTr("%1 reposted your repost").arg(authorName); break
            case "follow": summary = qsTr("%1 followed you").arg(authorName); break
            case "mention": summary = qsTr("%1 mentioned you").arg(authorName); break
            case "reply": summary = qsTr("%1 replied to you").arg(authorName); break
            case "quote": summary = qsTr("%1 quoted your post").arg(authorName); break
            case "starterpack-joined": summary = qsTr("%1 joined via your starter pack").arg(authorName); break
            default: summary = qsTr("New notifications"); break
            }
            systemNotification.previewSummary = summary
            systemNotification.previewBody = text
            systemNotification.summary = summary
            systemNotification.body = text.length > 0 ? text
                    : qsTr("%1 unread notifications").arg(count)
            systemNotification.itemCount = count
            systemNotification.publish()
        }
    }

    Notification {
        id: systemNotification
        appName: "Ice Age"
        appIcon: "harbour-iceage"
    }

    initialPage: Component {
        TimelinePage {
            session: blueskySession
            timeline: timelineModel
            notifications: notificationsModel
        }
    }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}
