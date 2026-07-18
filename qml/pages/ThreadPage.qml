import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"

Page {
    id: page

    property Session session
    property alias postUri: threadModel.postUri

    ThreadModel {
        id: threadModel
        session: page.session
    }

    Component.onCompleted: threadModel.load()

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: threadModel

        header: PageHeader {
            title: qsTr("Thread")
        }

        PullDownMenu {
            busy: threadModel.loading
            MenuItem {
                text: qsTr("Refresh")
                onClicked: threadModel.load()
            }
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: threadModel.loading && listView.count === 0
        }

        delegate: PostDelegate {
            feedModel: threadModel
        }

        VerticalScrollDecorator { }
    }
}
