# NOTICE:
#
# Application name defined in TARGET has a corresponding QML filename.
# If name defined in TARGET is changed, the following needs to be done
# to match new name:
#   - corresponding QML filename must be changed
#   - desktop icon filename must be changed
#   - desktop filename must be changed
#   - icon definition filename in desktop file must be changed
#   - translation filenames have to be changed

# The name of your application
TARGET = harbour-iceage

CONFIG += sailfishapp c++11 link_pkgconfig

QT += network

PKGCONFIG += sailfishsecrets sailfishapp openssl

SOURCES += src/harbour-iceage.cpp \
    src/xrpcclient.cpp \
    src/session.cpp \
    src/dpopkey.cpp \
    src/oauthflow.cpp \
    src/secretsstore.cpp \
    src/post.cpp \
    src/abstractpostmodel.cpp \
    src/timelinemodel.cpp \
    src/threadmodel.cpp \
    src/authorfeedmodel.cpp \
    src/notificationsmodel.cpp \
    src/composer.cpp \
    src/searchpostsmodel.cpp \
    src/actormodelbase.cpp \
    src/actorsearchmodel.cpp \
    src/actorlistmodel.cpp \
    src/feedlistmodel.cpp \
    src/mediadownloader.cpp \
    src/typeahead.cpp \
    src/tagstore.cpp

HEADERS += src/xrpcclient.h \
    src/session.h \
    src/dpopkey.h \
    src/oauthflow.h \
    src/secretsstore.h \
    src/post.h \
    src/abstractpostmodel.h \
    src/timelinemodel.h \
    src/threadmodel.h \
    src/authorfeedmodel.h \
    src/notificationsmodel.h \
    src/composer.h \
    src/searchpostsmodel.h \
    src/actormodelbase.h \
    src/actorsearchmodel.h \
    src/actorlistmodel.h \
    src/feedlistmodel.h \
    src/mediadownloader.h \
    src/typeahead.h \
    src/tagstore.h

DISTFILES += qml/harbour-iceage.qml \
    qml/cover/CoverPage.qml \
    qml/components/PostDelegate.qml \
    qml/pages/TimelinePage.qml \
    qml/pages/LoginPage.qml \
    qml/pages/ImagePage.qml \
    qml/pages/ThreadPage.qml \
    qml/pages/ComposePage.qml \
    qml/pages/ProfilePage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/NotificationsPage.qml \
    qml/pages/VideoPage.qml \
    qml/pages/SearchPage.qml \
    qml/pages/ActorListPage.qml \
    qml/pages/FeedsPage.qml \
    qml/components/ProfileFeedPane.qml \
    qml/components/ActorDelegate.qml \
    qml/components/linkutils.js \
    rpm/harbour-iceage.changes.in \
    rpm/harbour-iceage.changes.run.in \
    rpm/harbour-iceage.spec \
    translations/*.ts \
    harbour-iceage.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

# to disable building translations every time, comment out the
# following CONFIG line
CONFIG += sailfishapp_i18n

# German translation is enabled as an example. If you aren't
# planning to localize your app, remember to comment out the
# following TRANSLATIONS line. And also do not forget to
# modify the localized app name in the the .desktop file.
TRANSLATIONS += translations/harbour-iceage-de.ts
