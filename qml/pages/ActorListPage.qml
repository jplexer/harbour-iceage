import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0
import "../components"

// Followers or following of one account, per the mode property.
Page {
    id: page

    property Session session
    property string actor
    property string mode: "followers"

    ActorListModel {
        id: listModel
        session: page.session
        actor: page.actor
        mode: page.mode
    }

    Component.onCompleted: listModel.refresh()

    SilicaListView {
        id: listView
        anchors.fill: parent
        model: listModel

        header: PageHeader {
            title: page.mode === "following" ? qsTr("Following") : qsTr("Followers")
        }

        PullDownMenu {
            busy: listModel.loading
            MenuItem {
                text: qsTr("Refresh")
                onClicked: listModel.refresh()
            }
        }

        delegate: ActorDelegate {
            session: page.session
        }

        ViewPlaceholder {
            enabled: !listModel.loading && listView.count === 0
            text: page.mode === "following"
                  ? qsTr("Not following anyone") : qsTr("No followers yet")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: listModel.loading && listView.count === 0
        }

        onAtYEndChanged: {
            if (atYEnd && count > 0) {
                listModel.loadMore()
            }
        }

        VerticalScrollDecorator { }
    }
}
