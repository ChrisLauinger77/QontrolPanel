#include "panelengine.h"
#include "logmanager.h"
#include "windowsbackdrop.h"
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

class WindowsAppRuntimeGuard
{
public:
    WindowsAppRuntimeGuard()
    {
        WindowsBackdrop::initializeRuntime();
    }

    ~WindowsAppRuntimeGuard()
    {
        WindowsBackdrop::shutdownRuntime();
    }
};

enum class InstanceWaitResult {
    ActivatedExistingInstance,
    AcquiredInstanceLock,
    TimedOut
};

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

InstanceWaitResult waitForExistingInstance(QLockFile& instanceLock)
{
    QElapsedTimer timer;
    timer.start();

    do {
        if (tryConnectToExistingInstance(kServerConnectionTimeoutMs)) {
            return InstanceWaitResult::ActivatedExistingInstance;
        }

        if (instanceLock.tryLock(0)) {
            return InstanceWaitResult::AcquiredInstanceLock;
        }

        QThread::msleep(kServerRetryIntervalMs);
    } while (timer.elapsed() < kServerStartupTimeoutMs);

    if (instanceLock.tryLock(0)) {
        return InstanceWaitResult::AcquiredInstanceLock;
    }

    return InstanceWaitResult::TimedOut;
}

}

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(L"ChrisLauinger77.QontrolPanel");
#endif

    const WindowsAppRuntimeGuard windowsAppRuntime;

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
        const auto waitResult = waitForExistingInstance(instanceLock);
        if (waitResult == InstanceWaitResult::ActivatedExistingInstance) {
            LOG_INFO("LocalServer", "Another instance finished starting");
            return 0;
        }

        if (waitResult == InstanceWaitResult::TimedOut) {
            LOG_WARN("LocalServer", "Timed out waiting for the existing instance");
            return 0;
        }

        LOG_INFO("LocalServer", "Previous instance exited while relaunching");
    }

    PanelEngine w;

    return a.exec();
}
