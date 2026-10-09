import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import harbour.iceage 1.0

Page {
    id: page

    property Session session
    property string replyParentUri
    property string replyParentCid
    property string replyRootUri
    property string replyRootCid
    property string replyToHandle
    property string quoteUri
    property string quoteCid
    property string quoteAuthorHandle

    readonly property int maxLength: 300
    readonly property bool isReply: replyParentUri.length > 0
    readonly property bool isQuote: quoteUri.length > 0

    // Autocomplete for the @mention / #hashtag token at the cursor
    property int _tokenStart: 0
    property var _tagSuggestions: []

    function updateSuggestions() {
        var pos = textArea.cursorPosition
        var text = textArea.text
        var start = pos
        while (start > 0 && !/[\s(]/.test(text.charAt(start - 1)))
            start--
        page._tokenStart = start
        var word = text.substring(start, pos)
        if (word.length >= 2 && word.charAt(0) === '@') {
            typeahead.query(word.substring(1))
            page._tagSuggestions = []
        } else if (word.length >= 2 && word.charAt(0) === '#') {
            page._tagSuggestions = composer.suggestTags(word.substring(1))
            typeahead.clear()
        } else {
            typeahead.clear()
            page._tagSuggestions = []
        }
    }

    function applySuggestion(replacement) {
        var text = textArea.text
        var pos = textArea.cursorPosition
        textArea.text = text.substring(0, page._tokenStart) + replacement + " "
                + text.substring(pos)
        textArea.cursorPosition = page._tokenStart + replacement.length + 1
        typeahead.clear()
        page._tagSuggestions = []
    }

    Composer {
        id: composer
        session: page.session
        onPosted: pageStack.pop()
        onPostError: errorLabel.text = message
    }

    Typeahead {
        id: typeahead
        session: page.session
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: page.isReply ? qsTr("Reply to @%1").arg(replyToHandle)
                     : page.isQuote ? qsTr("Quote @%1").arg(quoteAuthorHandle)
                                    : qsTr("New post")
            }

            TextArea {
                id: textArea
                width: parent.width
                placeholderText: page.isReply ? qsTr("Write your reply") : qsTr("What's up?")
                focus: true
                // Predictive text holds keystrokes in the keyboard's
                // pre-edit buffer, so text/cursorPosition would only
                // update at word boundaries and autocomplete would
                // never see a partial @handle or #tag
                inputMethodHints: Qt.ImhNoPredictiveText
                onTextChanged: page.updateSuggestions()
                onCursorPositionChanged: page.updateSuggestions()
            }

            Column {
                width: parent.width
                visible: typeahead.suggestions.length > 0
                         || page._tagSuggestions.length > 0

                Repeater {
                    model: typeahead.suggestions

                    BackgroundItem {
                        width: parent.width
                        height: Theme.itemSizeSmall
                        onClicked: page.applySuggestion("@" + modelData.handle)

                        Image {
                            id: suggestionAvatar
                            x: Theme.horizontalPageMargin
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.iconSizeSmall
                            height: Theme.iconSizeSmall
                            source: modelData.avatar
                            asynchronous: true
                            fillMode: Image.PreserveAspectCrop
                        }

                        Label {
                            anchors {
                                left: suggestionAvatar.right
                                leftMargin: Theme.paddingMedium
                                right: parent.right
                                rightMargin: Theme.horizontalPageMargin
                                verticalCenter: parent.verticalCenter
                            }
                            truncationMode: TruncationMode.Fade
                            text: modelData.displayName
                                  + " · @" + modelData.handle
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.primaryColor
                        }
                    }
                }

                Repeater {
                    model: page._tagSuggestions

                    BackgroundItem {
                        width: parent.width
                        height: Theme.itemSizeSmall
                        onClicked: page.applySuggestion("#" + modelData)

                        Label {
                            x: Theme.horizontalPageMargin
                            anchors.verticalCenter: parent.verticalCenter
                            text: "#" + modelData
                            font.pixelSize: Theme.fontSizeSmall
                            color: Theme.highlightColor
                        }
                    }
                }
            }

            Label {
                anchors.right: parent.right
                anchors.rightMargin: Theme.horizontalPageMargin
                text: (page.maxLength - textArea.text.length)
                font.pixelSize: Theme.fontSizeExtraSmall
                color: textArea.text.length > page.maxLength ? Theme.errorColor
                                                             : Theme.secondaryColor
            }

            // Attached images; tap a thumbnail to edit its alt text,
            // tap the cross to remove it
            Flow {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                spacing: Theme.paddingSmall
                visible: composer.images.length > 0

                Repeater {
                    model: composer.images

                    Image {
                        width: Theme.itemSizeLarge
                        height: Theme.itemSizeLarge
                        source: modelData
                        sourceSize.width: Theme.itemSizeLarge
                        sourceSize.height: Theme.itemSizeLarge
                        asynchronous: true
                        fillMode: Image.PreserveAspectCrop

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var imageIndex = index
                                var dialog = pageStack.push(
                                        Qt.resolvedUrl("AltTextDialog.qml"), {
                                    imagePath: modelData,
                                    alt: composer.imageAlts[index]
                                })
                                dialog.accepted.connect(function() {
                                    composer.setImageAlt(imageIndex, dialog.alt)
                                })
                            }
                        }

                        // "ALT" badge once alt text is set
                        Rectangle {
                            anchors {
                                left: parent.left
                                bottom: parent.bottom
                            }
                            width: altBadge.width + 2 * Theme.paddingSmall
                            height: altBadge.height + Theme.paddingSmall
                            color: Theme.rgba("black", 0.6)
                            visible: composer.imageAlts[index].length > 0

                            Label {
                                id: altBadge
                                anchors.centerIn: parent
                                text: "ALT"
                                font.pixelSize: Theme.fontSizeTiny
                                color: Theme.primaryColor
                            }
                        }

                        IconButton {
                            anchors {
                                top: parent.top
                                right: parent.right
                            }
                            width: Theme.iconSizeMedium
                            height: Theme.iconSizeMedium
                            icon.source: "image://theme/icon-m-clear"
                            onClicked: composer.removeImage(index)
                        }
                    }
                }
            }

            ButtonLayout {
                Button {
                    text: qsTr("Add images")
                    visible: composer.images.length < 4
                    enabled: !composer.busy
                    onClicked: pageStack.push(imagePicker)
                }
            }

            // Threadgates belong to the thread root, so only new posts
            ComboBox {
                readonly property var gates: ["", "following", "followers", "mentioned", "nobody"]
                width: parent.width
                visible: !page.isReply
                enabled: !composer.busy
                label: qsTr("Who can reply")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Everyone") }
                    MenuItem { text: qsTr("People you follow") }
                    MenuItem { text: qsTr("Your followers") }
                    MenuItem { text: qsTr("People you mention") }
                    MenuItem { text: qsTr("Nobody") }
                }
                onCurrentIndexChanged: composer.replyGate = gates[currentIndex]
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
                         && (textArea.text.trim().length > 0
                             || composer.images.length > 0
                             || page.isQuote)
                         && textArea.text.length <= page.maxLength
                onClicked: {
                    errorLabel.text = ""
                    composer.post(textArea.text, replyParentUri, replyParentCid,
                                  replyRootUri, replyRootCid, quoteUri, quoteCid)
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

    Component {
        id: imagePicker

        MultiImagePickerDialog {
            onAccepted: {
                for (var i = 0; i < selectedContent.count; ++i)
                    composer.addImage(selectedContent.get(i).filePath)
            }
        }
    }
}
