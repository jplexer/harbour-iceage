#ifdef QT_QML_DEBUG
#include <QtQuick>
#endif

#include <QGuiApplication>
#include <QQuickView>
#include <QScopedPointer>
#include <QtQml>

#include <sailfishapp.h>

#include "authorfeedmodel.h"
#include "composer.h"
#include "notificationsmodel.h"
#include "session.h"
#include "threadmodel.h"
#include "timelinemodel.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    // Must match OrganizationName/ApplicationName in the .desktop [X-Sailjail]
    // section, so QSettings lands in the sandbox-writable config dir.
    app->setOrganizationName(QStringLiteral("me.funnyna"));
    app->setApplicationName(QStringLiteral("harbour-iceage"));

    // Harbour only allows private QML imports under harbour.<appname>
    qmlRegisterType<Session>("harbour.iceage", 1, 0, "Session");
    qmlRegisterType<TimelineModel>("harbour.iceage", 1, 0, "TimelineModel");
    qmlRegisterType<ThreadModel>("harbour.iceage", 1, 0, "ThreadModel");
    qmlRegisterType<AuthorFeedModel>("harbour.iceage", 1, 0, "AuthorFeedModel");
    qmlRegisterType<NotificationsModel>("harbour.iceage", 1, 0, "NotificationsModel");
    qmlRegisterType<Composer>("harbour.iceage", 1, 0, "Composer");

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
