#include "panelengine.h"
#include "logmanager.h"
#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QProcess>
#include <QLocalSocket>
#include <QLocalServer>
#include <QLoggingCategory>

#ifdef Q_OS_WIN
#include <shobjidl_core.h>
#endif

namespace {

constexpr auto kLocalServerName = "QontrolPanel";

bool tryConnectToExistingInstance()
{
    QLocalSocket socket;
    socket.connectToServer(kLocalServerName);

    if (socket.waitForConnected(1000)) {
        socket.write("show_panel");
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return true;
    }

    return false;
}

}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"ChrisLauinger77.QontrolPanel");
#endif

    QLoggingCategory::setFilterRules(
        "qt.multimedia.*=false\n"
        "qt.qpa.mime*=false"
        );

    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
    a.setOrganizationName("ChrisLauinger77");
    a.setApplicationName("QontrolPanel");
    qRegisterMetaType<LogManager::LogType>("LogType");
    LOG_INFO("Core", "Starting application");

    if (tryConnectToExistingInstance()) {
        LOG_INFO("LocalServer", "Another instance is already running");
        return 0;
    }

    QLockFile instanceLock(QDir(QDir::tempPath()).filePath("QontrolPanel.instance.lock"));
    if (!instanceLock.tryLock(100)) {
        // The primary process may still be starting and not listening yet.
        tryConnectToExistingInstance();
        LOG_INFO("LocalServer", "Another instance is already starting");
        return 0;
    }

    PanelEngine w;

    return a.exec();
}
