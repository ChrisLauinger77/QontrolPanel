#include "panelengine.h"
#include "logmanager.h"
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QLockFile>
#include <QProcess>
#include <QLocalSocket>
#include <QLocalServer>
#include <QLoggingCategory>
#include <QThread>

#ifdef Q_OS_WIN
#include <shobjidl_core.h>
#endif

namespace {

constexpr auto kLocalServerName = "QontrolPanel";
constexpr auto kServerStartupTimeoutMs = 5000;
constexpr auto kServerRetryIntervalMs = 50;
constexpr auto kServerConnectionTimeoutMs = 250;

bool tryConnectToExistingInstance(int timeoutMs = 1000)
{
    QLocalSocket socket;
    socket.connectToServer(kLocalServerName);

    if (socket.waitForConnected(timeoutMs)) {
        socket.write("show_panel");
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return true;
    }

    return false;
}

bool waitForExistingInstance()
{
    QElapsedTimer timer;
    timer.start();

    do {
        if (tryConnectToExistingInstance(kServerConnectionTimeoutMs)) {
            return true;
        }

        QThread::msleep(kServerRetryIntervalMs);
    } while (timer.elapsed() < kServerStartupTimeoutMs);

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
        if (waitForExistingInstance()) {
            LOG_INFO("LocalServer", "Another instance finished starting");
        } else {
            LOG_WARN("LocalServer", "Timed out waiting for the existing instance");
        }
        return 0;
    }

    PanelEngine w;

    return a.exec();
}
