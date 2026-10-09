import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    id: cover

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
        default: return name
        }
    }

    Image {
        source: "iceberg.png"
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
        }
        // Oversized on purpose; the sides cropping off is fine
        width: parent.width * 2.25
        fillMode: Image.PreserveAspectFit
        opacity: 0.15
    }

    Label {
        id: handleLabel
        anchors {
            top: parent.top
            topMargin: Theme.paddingLarge
            left: parent.left
            leftMargin: Theme.paddingLarge
            right: parent.right
            rightMargin: Theme.paddingLarge
        }
        truncationMode: TruncationMode.Fade
        text: blueskySession.loggedIn ? "@" + blueskySession.handle : "Ice Age"
        font.pixelSize: Theme.fontSizeSmall
        color: Theme.highlightColor
    }

    ListView {
        anchors {
            top: handleLabel.bottom
            topMargin: Theme.paddingMedium
            left: parent.left
            leftMargin: Theme.paddingLarge
            right: parent.right
            rightMargin: Theme.paddingLarge
            bottom: parent.bottom
            bottomMargin: Theme.itemSizeSmall // keep clear of cover actions
        }
        model: notificationsModel
        interactive: false
        clip: true

        // Only unread notifications; read items collapse to zero height
        delegate: Item {
            width: ListView.view.width
            visible: !isRead
            height: isRead ? 0 : label.height + Theme.paddingSmall

            Label {
                id: label
                width: parent.width
                wrapMode: Text.Wrap
                maximumLineCount: 2
                truncationMode: TruncationMode.Elide
                text: cover.reasonText(reason, authorName)
                font.pixelSize: Theme.fontSizeTiny
                color: Theme.primaryColor
            }
        }
    }

    CoverActionList {
        enabled: blueskySession.loggedIn

        CoverAction {
            iconSource: "image://theme/icon-cover-new"
            onTriggered: {
                pageStack.push(Qt.resolvedUrl("../pages/ComposePage.qml"),
                               { session: blueskySession }, PageStackAction.Immediate)
                app.activate()
            }
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-refresh"
            onTriggered: {
                timelineModel.loadNewer()
                notificationsModel.poll()
                notificationsModel.refresh()
            }
        }
    }
}
