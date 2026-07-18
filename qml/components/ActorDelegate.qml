import QtQuick 2.0
import Sailfish.Silica 1.0

// One profile row in an actor list (search results, followers,
// following). Expects the roles of ActorModelBase; tap opens the profile.
BackgroundItem {
    id: actorItem

    property var session

    width: ListView.view.width
    height: Math.max(actorColumn.height + 2 * Theme.paddingMedium,
                     Theme.itemSizeMedium)

    onClicked: pageStack.push(Qt.resolvedUrl("../pages/ProfilePage.qml"), {
        session: actorItem.session,
        actor: handle
    })

    Image {
        id: actorAvatar
        x: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.itemSizeExtraSmall
        height: Theme.itemSizeExtraSmall
        source: avatar
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
    }

    Column {
        id: actorColumn
        anchors {
            left: actorAvatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }

        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: displayName
            font.pixelSize: Theme.fontSizeSmall
            color: actorItem.highlighted
                   ? Theme.highlightColor : Theme.primaryColor
        }

        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: followsYou
                  ? "@" + handle + " · " + qsTr("Follows you")
                  : "@" + handle
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
        }

        Label {
            width: parent.width
            visible: text.length > 0
            truncationMode: TruncationMode.Fade
            text: description
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }
}
