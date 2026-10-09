import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.iceage 1.0

Page {
    id: page

    property Session session

    Connections {
        target: session
        onLoggedInChanged: {
            if (session.loggedIn) {
                pageStack.pop()
            }
        }
        onLoginError: errorLabel.text = message
    }

    // Backing out mid-login abandons it rather than leaving the
    // timeline spinning behind us. This page pushes nothing, so leaving it
    // always means it's being popped.
    onStatusChanged: {
        if (status === PageStatus.Deactivating && !session.loggedIn)
            session.cancelLogin()
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: qsTr("Sign in to Bluesky")
            }

            TextField {
                id: handleField
                width: parent.width
                label: qsTr("Handle")
                placeholderText: qsTr("alice.bsky.social")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.enabled: text.length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    errorLabel.text = ""
                    session.loginWithHandle(handleField.text)
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Sign in")
                enabled: !session.busy && handleField.text.length > 0
                onClicked: {
                    errorLabel.text = ""
                    session.loginWithHandle(handleField.text)
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Your server's sign-in page opens in the browser.")
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Medium
                running: session.busy
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Cancel")
                visible: session.busy
                onClicked: session.cancelLogin()
            }

            Label {
                id: errorLabel
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.errorColor
                visible: text.length > 0
            }

            TextSwitch {
                id: advancedSwitch
                text: qsTr("Advanced")
                description: qsTr("Manual server or app-password sign-in")
            }

            Column {
                width: parent.width
                spacing: Theme.paddingMedium
                visible: advancedSwitch.checked

                TextField {
                    id: serviceField
                    width: parent.width
                    label: qsTr("Server")
                    text: session ? session.service : ""
                    placeholderText: "https://bsky.social"
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                                      | Qt.ImhNoPredictiveText
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Sign in with browser at this server")
                    enabled: !session.busy
                    onClicked: {
                        errorLabel.text = ""
                        session.service = serviceField.text
                        session.loginOAuth()
                    }
                }

                SectionHeader {
                    text: qsTr("App password")
                }

                TextField {
                    id: identifierField
                    width: parent.width
                    label: qsTr("Handle or email")
                    placeholderText: label
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                    EnterKey.iconSource: "image://theme/icon-m-enter-next"
                    EnterKey.onClicked: passwordField.focus = true
                }

                PasswordField {
                    id: passwordField
                    width: parent.width
                    label: qsTr("App password")
                    EnterKey.enabled: text.length > 0
                    EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                    EnterKey.onClicked: page.appPasswordSignIn()
                }

                Button {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Sign in with app password")
                    enabled: !session.busy
                             && identifierField.text.length > 0
                             && passwordField.text.length > 0
                    onClicked: page.appPasswordSignIn()
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: qsTr("Use an app password, not your account password. "
                               + "Create one on bsky.app under Settings → App Passwords.")
                }
            }
        }

        VerticalScrollDecorator { }
    }

    function appPasswordSignIn() {
        errorLabel.text = ""
        session.service = serviceField.text
        session.login(identifierField.text, passwordField.text)
    }
}
