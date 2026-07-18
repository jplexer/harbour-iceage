import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"

Page {
    id: page

    property Session session
    // Pre-filled query (e.g. from a tapped hashtag); searches immediately
    property string initialQuery

    Component.onCompleted: {
        if (initialQuery.length > 0) {
            searchField.text = initialQuery
            runSearch()
        }
    }

    SearchPostsModel {
        id: searchPosts
        session: page.session
    }

    ActorSearchModel {
        id: searchActors
        session: page.session
    }

    function runSearch() {
        searchPosts.query = searchField.text
        searchActors.query = searchField.text
        searchPosts.search()
        searchActors.search()
    }

    Column {
        id: headerColumn
        width: parent.width

        PageHeader {
            title: qsTr("Search")
        }

        SearchField {
            id: searchField
            width: parent.width
            placeholderText: qsTr("Posts, people, #hashtags")

            EnterKey.enabled: text.length > 0
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: {
                focus = false
                page.runSearch()
            }
        }

        Row {
            id: tabRow
            width: parent.width

            Repeater {
                model: [qsTr("Posts"), qsTr("People")]

                BackgroundItem {
                    width: parent.width / 2
                    height: Theme.itemSizeSmall
                    onClicked: pager.currentIndex = index

                    Label {
                        anchors.centerIn: parent
                        text: modelData
                        font.pixelSize: Theme.fontSizeSmall
                        color: pager.currentIndex === index
                               ? Theme.highlightColor : Theme.secondaryColor
                    }

                    Rectangle {
                        anchors {
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                        }
                        height: Math.round(Theme.pixelRatio * 2)
                        color: Theme.highlightColor
                        visible: pager.currentIndex === index
                    }
                }
            }
        }
    }

    ListView {
        id: pager
        anchors {
            top: headerColumn.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 300
        boundsBehavior: Flickable.DragOverBounds
        clip: true
        model: 2

        // Swiping right past the first pane navigates back
        onDraggingChanged: {
            if (!dragging && currentIndex === 0
                    && contentX < originX - Theme.itemSizeSmall / 2)
                pageStack.pop()
        }

        delegate: Loader {
            width: pager.width
            height: pager.height
            sourceComponent: index === 0 ? postsPane : peoplePane
        }
    }

    Component {
        id: postsPane

        SilicaListView {
            model: searchPosts
            clip: true

            delegate: PostDelegate {
                feedModel: searchPosts
            }

            ViewPlaceholder {
                enabled: searchPosts.query.length > 0 && !searchPosts.loading
                         && parent.count === 0
                text: qsTr("No posts found")
            }

            BusyIndicator {
                anchors.centerIn: parent
                size: BusyIndicatorSize.Large
                running: searchPosts.loading && parent.count === 0
            }

            onAtYEndChanged: {
                if (atYEnd && count > 0) {
                    searchPosts.loadMore()
                }
            }

            VerticalScrollDecorator { }
        }
    }

    Component {
        id: peoplePane

        SilicaListView {
            model: searchActors
            clip: true

            delegate: ActorDelegate {
                session: page.session
            }

            ViewPlaceholder {
                enabled: searchActors.query.length > 0 && !searchActors.loading
                         && parent.count === 0
                text: qsTr("No people found")
            }

            BusyIndicator {
                anchors.centerIn: parent
                size: BusyIndicatorSize.Large
                running: searchActors.loading && parent.count === 0
            }

            onAtYEndChanged: {
                if (atYEnd && count > 0) {
                    searchActors.loadMore()
                }
            }

            VerticalScrollDecorator { }
        }
    }
}
