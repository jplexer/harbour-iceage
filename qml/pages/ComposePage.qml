import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0

Page {
    id: page

    property Session session
    property string replyParentUri
    property string replyParentCid
    property string replyRootUri
    property string replyRootCid
    property string replyToHandle

    readonly property int maxLength: 300
    readonly property bool isReply: replyParentUri.length > 0

    Composer {
        id: composer
        session: page.session
        onPosted: pageStack.pop()
        onPostError: errorLabel.text = message
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: page.isReply ? qsTr("Reply to @%1").arg(replyToHandle) : qsTr("New post")
            }

            TextArea {
                id: textArea
                width: parent.width
                placeholderText: page.isReply ? qsTr("Write your reply") : qsTr("What's up?")
                focus: true
            }

            Label {
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                text: (page.maxLength - textArea.text.length)
                font.pixelSize: Theme.fontSizeExtraSmall
                color: textArea.text.length > page.maxLength ? Theme.errorColor
                                                             : Theme.secondaryColor
            }

            Label {
                id: errorLabel
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.errorColor
                visible: text.length > 0
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: page.isReply ? qsTr("Reply") : qsTr("Post")
                enabled: !composer.busy
                         && textArea.text.trim().length > 0
                         && textArea.text.length <= page.maxLength
                onClicked: {
                    errorLabel.text = ""
                    composer.post(textArea.text, replyParentUri, replyParentCid,
                                  replyRootUri, replyRootCid)
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: composer.busy
            }
        }

        VerticalScrollDecorator { }
    }
}
