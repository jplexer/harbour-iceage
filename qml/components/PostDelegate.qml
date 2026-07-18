import QtQuick 2.0
import Sailfish.Silica 1.0

// Shared delegate for post list models (timeline, thread, author feed).
// Expects the roles of AbstractPostModel; set feedModel to the model
// instance for like/repost/reply actions (in the long-press menu).
ListItem {
    id: postItem

    property var feedModel
    property bool openThreadOnClick: true

    readonly property real indent: depth * Theme.paddingLarge

    width: ListView.view.width
    contentHeight: Math.max(contentColumn.y + contentColumn.height,
                            avatar.y + avatar.height) + Theme.paddingMedium

    function openProfile() {
        pageStack.push(Qt.resolvedUrl("../pages/ProfilePage.qml"), {
            session: feedModel.session,
            actor: authorHandle
        })
    }

    onClicked: {
        if (openThreadOnClick && !isMain) {
            pageStack.push(Qt.resolvedUrl("../pages/ThreadPage.qml"), {
                postUri: uri,
                session: feedModel.session
            })
        }
    }

    menu: ContextMenu {
        MenuItem {
            text: qsTr("Reply (%1)").arg(replyCount)
            onClicked: pageStack.push(Qt.resolvedUrl("../pages/ComposePage.qml"), {
                session: feedModel.session,
                replyParentUri: uri,
                replyParentCid: cid,
                replyRootUri: replyRootUri,
                replyRootCid: replyRootCid,
                replyToHandle: authorHandle
            })
        }
        MenuItem {
            text: (repostUri.length > 0 ? qsTr("Undo repost (%1)") : qsTr("Repost (%1)"))
                  .arg(repostCount)
            onClicked: feedModel.toggleRepost(index)
        }
        MenuItem {
            text: (likeUri.length > 0 ? qsTr("Unlike (%1)") : qsTr("Like (%1)"))
                  .arg(likeCount)
            onClicked: feedModel.toggleLike(index)
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: isMain
        color: Theme.rgba(Theme.highlightBackgroundColor, Theme.highlightBackgroundOpacity * 0.4)
    }

    // Context lines span the full width above the avatar row, so the avatar
    // always aligns with the name/handle.
    Column {
        id: contextColumn
        anchors {
            left: parent.left
            leftMargin: Theme.horizontalPageMargin + postItem.indent
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            top: parent.top
            topMargin: Theme.paddingMedium
        }

        Label {
            visible: repostedBy.length > 0
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: qsTr("Reposted by %1").arg(repostedBy)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }

        Label {
            // Thread pages show replies in context already (depth > 0)
            visible: replyToHandle.length > 0 && depth === 0 && !isMain
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: qsTr("Replying to @%1").arg(replyToHandle)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryHighlightColor
        }
    }

    Image {
        id: avatar
        x: Theme.horizontalPageMargin + postItem.indent
        anchors.top: contextColumn.bottom
        anchors.topMargin: contextColumn.height > 0 ? Theme.paddingSmall : 0
        width: Theme.itemSizeExtraSmall
        height: Theme.itemSizeExtraSmall
        source: authorAvatar
        asynchronous: true
        fillMode: Image.PreserveAspectCrop

        MouseArea {
            anchors.fill: parent
            onClicked: postItem.openProfile()
        }
    }

    Column {
        id: contentColumn
        anchors {
            left: avatar.right
            leftMargin: Theme.paddingMedium
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            top: avatar.top
        }

        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: authorName
            font.pixelSize: Theme.fontSizeSmall
            color: postItem.highlighted ? Theme.highlightColor : Theme.primaryColor

            MouseArea {
                anchors.fill: parent
                onClicked: postItem.openProfile()
            }
        }

        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            text: "@" + authorHandle + " · "
                  + Format.formatDate(createdAt, Formatter.DurationElapsed)
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor

            MouseArea {
                anchors.fill: parent
                onClicked: postItem.openProfile()
            }
        }

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            textFormat: Text.StyledText
            text: bodyHtml
            visible: text.length > 0
            font.pixelSize: Theme.fontSizeSmall
            color: postItem.highlighted ? Theme.highlightColor : Theme.primaryColor
            linkColor: Theme.highlightColor
            onLinkActivated: Qt.openUrlExternally(link)
        }

        Repeater {
            model: images
            delegate: Image {
                width: contentColumn.width
                height: modelData.aspectWidth > 0
                        ? width * modelData.aspectHeight / modelData.aspectWidth
                        : (sourceSize.width > 0 ? width * sourceSize.height / sourceSize.width
                                                : Theme.itemSizeLarge)
                source: modelData.thumb
                fillMode: Image.PreserveAspectFit
                asynchronous: true

                MouseArea {
                    anchors.fill: parent
                    onClicked: pageStack.push(Qt.resolvedUrl("../pages/ImagePage.qml"), {
                        source: modelData.fullsize,
                        altText: modelData.alt
                    })
                }
            }
        }

        // Video embed: thumbnail with play overlay; playback on its own page
        Item {
            visible: video.playlist !== undefined
            width: parent.width
            height: !visible ? 0
                    : (video.aspectWidth > 0
                       ? width * video.aspectHeight / video.aspectWidth
                       : width * 9 / 16)

            Image {
                anchors.fill: parent
                source: video.thumbnail !== undefined ? video.thumbnail : ""
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
            }

            Rectangle {
                anchors.fill: parent
                color: Theme.rgba("black", 0.3)
            }

            Image {
                anchors.centerIn: parent
                source: "image://theme/icon-l-play?" + Theme.primaryColor
            }

            MouseArea {
                anchors.fill: parent
                onClicked: pageStack.push(Qt.resolvedUrl("../pages/VideoPage.qml"), {
                    source: video.playlist,
                    altText: video.alt !== undefined ? video.alt : ""
                })
            }
        }

        // Quote post
        Rectangle {
            visible: quote.authorHandle !== undefined
            width: parent.width
            height: quoteColumn.height + 2 * Theme.paddingSmall
            radius: Theme.paddingSmall / 2
            color: Theme.rgba(Theme.secondaryHighlightColor, 0.1)
            border.color: Theme.rgba(Theme.secondaryColor, 0.3)
            border.width: 1

            Column {
                id: quoteColumn
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: Theme.paddingSmall
                }

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: visible ? quote.authorName + " @" + quote.authorHandle : ""
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryHighlightColor
                }
                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    maximumLineCount: 6
                    truncationMode: TruncationMode.Elide
                    text: visible && quote.text !== undefined ? quote.text : ""
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: pageStack.push(Qt.resolvedUrl("../pages/ThreadPage.qml"), {
                    postUri: quote.uri,
                    session: feedModel.session
                })
            }
        }

        // External link card
        Rectangle {
            visible: external.uri !== undefined
            width: parent.width
            height: cardRow.height + 2 * Theme.paddingSmall
            radius: Theme.paddingSmall / 2
            color: Theme.rgba(Theme.secondaryHighlightColor, 0.1)
            border.color: Theme.rgba(Theme.secondaryColor, 0.3)
            border.width: 1

            Row {
                id: cardRow
                anchors {
                    left: parent.left
                    right: parent.right
                    top: parent.top
                    margins: Theme.paddingSmall
                }
                spacing: Theme.paddingSmall

                Image {
                    id: cardThumb
                    width: visible ? Theme.itemSizeMedium : 0
                    height: Theme.itemSizeMedium
                    visible: external.thumb !== undefined && external.thumb.length > 0
                    source: external.thumb !== undefined ? external.thumb : ""
                    asynchronous: true
                    fillMode: Image.PreserveAspectCrop
                }

                Column {
                    width: parent.width - (cardThumb.visible ? cardThumb.width + parent.spacing : 0)

                    Label {
                        width: parent.width
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        truncationMode: TruncationMode.Elide
                        text: external.title !== undefined ? external.title : ""
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.primaryColor
                    }
                    Label {
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        text: external.uri !== undefined ? external.uri : ""
                        font.pixelSize: Theme.fontSizeTiny
                        color: Theme.secondaryColor
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: if (external.uri !== undefined) Qt.openUrlExternally(external.uri)
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
