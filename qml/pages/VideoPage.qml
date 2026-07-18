import QtQuick 2.0
import Sailfish.Silica 1.0
import QtMultimedia 5.6
import harbour.iceage 1.0

Page {
    id: page

    property alias source: player.source
    property string altText
    // For downloading the original blob from the author's PDS
    property string videoDid
    property string videoCid

    allowedOrientations: Orientation.All

    MediaDownloader {
        id: downloader
        onFinished: Notices.show(qsTr("Saved to Videos"))
        onDownloadError: Notices.show(qsTr("Download failed: %1").arg(message))
    }

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    MediaPlayer {
        id: player
        autoPlay: true
    }

    VideoOutput {
        anchors.fill: parent
        source: player
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: height

        PullDownMenu {
            busy: downloader.busy
            MenuItem {
                visible: page.videoDid.length > 0 && page.videoCid.length > 0
                text: qsTr("Save video")
                enabled: !downloader.busy
                onClicked: downloader.downloadVideo(page.videoDid, page.videoCid)
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                if (player.playbackState === MediaPlayer.PlayingState) {
                    player.pause()
                } else {
                    player.play()
                }
            }
        }
    }

    Image {
        anchors.centerIn: parent
        source: "image://theme/icon-l-play?" + Theme.primaryColor
        visible: player.playbackState !== MediaPlayer.PlayingState
                 && player.status !== MediaPlayer.Loading
                 && player.status !== MediaPlayer.Buffering
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: player.status === MediaPlayer.Loading
                 || player.status === MediaPlayer.Buffering
                 || player.status === MediaPlayer.Stalled
    }

    Label {
        visible: player.error !== MediaPlayer.NoError
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Could not play video")
        color: Theme.errorColor
    }

    Slider {
        id: progressSlider
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        minimumValue: 0
        maximumValue: player.duration > 0 ? player.duration : 1
        enabled: player.seekable && player.duration > 0
        valueText: Format.formatDuration(Math.round(value / 1000),
                                         Formatter.DurationShort)
                   + " / "
                   + Format.formatDuration(Math.round(player.duration / 1000),
                                           Formatter.DurationShort)
        onReleased: player.seek(value)
    }

    Connections {
        target: player
        onPositionChanged: {
            // Don't fight the user's finger while scrubbing
            if (!progressSlider.pressed) {
                progressSlider.value = player.position
            }
        }
    }

    Label {
        visible: altText.length > 0
        anchors {
            left: parent.left
            right: parent.right
            bottom: progressSlider.top
            leftMargin: Theme.horizontalPageMargin
            rightMargin: Theme.horizontalPageMargin
        }
        wrapMode: Text.Wrap
        maximumLineCount: 3
        truncationMode: TruncationMode.Elide
        text: altText
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryColor
    }
}
