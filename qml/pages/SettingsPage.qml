import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0

Page {
    id: page

    property Session session

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Settings")
            }

            SectionHeader {
                text: qsTr("Account")
            }

            DetailItem {
                label: qsTr("Signed in as")
                value: session.loggedIn ? "@" + session.handle : qsTr("Not signed in")
            }

            DetailItem {
                label: qsTr("Server")
                value: session.service
            }

            DetailItem {
                visible: session.loggedIn
                label: qsTr("DID")
                value: session.did
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: session.loggedIn
                text: qsTr("Sign out")
                onClicked: signOutRemorse.execute(qsTr("Signing out"), function() {
                    session.logout()
                    pageStack.pop()
                })
            }

            RemorsePopup {
                id: signOutRemorse
            }
        }

        VerticalScrollDecorator { }
    }
}
