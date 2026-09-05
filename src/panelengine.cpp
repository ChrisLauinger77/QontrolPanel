#include "panelengine.h"
#include "mediasessionmanager.h"
#include "languagebridge.h"
#include "workerthreads.h"
#include "headsetcontrolbridge.h"
#include "trayiconprovider.h"
#include "usersettings.h"
#include <QMenu>
#include <QApplication>
#include <QScreen>
#include <QRect>
#include <QQuickWindow>
#include <QWindow>
#include <QQmlContext>
#include <QTimer>
#include <QFontMetrics>
#include <QVariant>
#include <Windows.h>
#include <QProcess>
#include <cstdlib>

HWINEVENTHOOK PanelEngine::focusHook = NULL;
PanelEngine* PanelEngine::instance = nullptr;

PanelEngine::PanelEngine(QWidget *parent)
    : QWidget(parent)
    , engine(nullptr)
    , panelWindow(nullptr)
    , mediaPanelWindow(nullptr)
    , localServer(nullptr)
{
    UserSettings::instance();
    bool enableMediaSessionManager = UserSettings::instance()->enableMediaSessionManager();
    if (enableMediaSessionManager) {
        MediaSessionManager::initialize();
    }

    instance = this;
    setupLocalServer();
    initializeQMLEngine();

    if (LanguageBridge::instance()) {
        connect(LanguageBridge::instance(), &LanguageBridge::languageChanged,
                this, &PanelEngine::onLanguageChanged);
    }

    LanguageBridge::instance()->changeApplicationLanguage(UserSettings::instance()->languageIndex());
}

PanelEngine::~PanelEngine()
{
    stopFocusMonitoring();
    MediaSessionManager::cleanup();
    destroyQMLEngine();
    HeadsetControlBridge::instance()->shutdown();
    cleanupLocalServer();
    if (!drainRetiredWorkerThreads())
        std::_Exit(EXIT_FAILURE);
    instance = nullptr;
}

void PanelEngine::initializeQMLEngine()
{
    if (engine) {
        return;
    }

    engine = new QQmlApplicationEngine(this);
    connect(
        engine, &QQmlApplicationEngine::objectCreationFailed, qApp, [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);
    engine->addImageProvider("trayicon", new TrayIconProvider());
    engine->loadFromModule("ChrisLauinger77.QontrolPanel", "Main");

    if (!engine->rootObjects().isEmpty()) {
        panelWindow = qobject_cast<QWindow*>(engine->rootObjects().constFirst());
        if (panelWindow) {
            if (auto* quickWindow = qobject_cast<QQuickWindow*>(panelWindow.data()))
            {
                // The main panel is animated fully off-screen before it is hidden.
                // Recreate its OpenGL scene graph on the next show so Windows/DWM
                // cannot retain a stale backing surface that only repaints on resize.
                quickWindow->setPersistentGraphics(false);
                quickWindow->setPersistentSceneGraph(false);
            }

            QObject* mediaWindowObject = panelWindow->property("mediaSurfaceWindow").value<QObject*>();
            mediaPanelWindow = qobject_cast<QWindow*>(mediaWindowObject);
            if (!mediaPanelWindow) {
                qWarning() << "Main media panel window was not found";
            }
            connect(panelWindow, &QWindow::visibleChanged,
                    this, &PanelEngine::onPanelVisibilityChanged);
        }
    }
}

void PanelEngine::onPanelVisibilityChanged(bool visible)
{
    isPanelVisible = visible;

    if (visible) {
        startFocusMonitoring();
        if (auto* quickWindow = qobject_cast<QQuickWindow*>(panelWindow.data()))
        {
            QTimer::singleShot(0, quickWindow, &QQuickWindow::update);
        }
    } else {
        stopFocusMonitoring();
    }
}

void PanelEngine::destroyQMLEngine()
{
    if (engine) {
        delete engine;
        engine = nullptr;
    }
    panelWindow = nullptr;
    mediaPanelWindow = nullptr;
}

bool PanelEngine::isPanelWindow(HWND windowHandle) const
{
    if (panelWindow && windowHandle == reinterpret_cast<HWND>(panelWindow->winId())) {
        return true;
    }

    return mediaPanelWindow
        && windowHandle == reinterpret_cast<HWND>(mediaPanelWindow->winId());
}

void PanelEngine::startFocusMonitoring()
{
    if (focusHook == NULL) {
        focusHook = SetWinEventHook(
            EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
            NULL,
            WinEventProc,
            0, 0,
            WINEVENT_OUTOFCONTEXT
        );
    }
}

void PanelEngine::stopFocusMonitoring()
{
    if (focusHook != NULL) {
        UnhookWinEvent(focusHook);
        focusHook = NULL;
    }
}

void CALLBACK PanelEngine::WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
                                         LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    Q_UNUSED(hWinEventHook)
    Q_UNUSED(idObject)
    Q_UNUSED(idChild)
    Q_UNUSED(dwEventThread)
    Q_UNUSED(dwmsEventTime)

    if (event == EVENT_SYSTEM_FOREGROUND && instance && instance->panelWindow && instance->isPanelVisible) {
        if (!instance->isPanelWindow(hwnd)) {
            instance->stopFocusMonitoring();
            QMetaObject::invokeMethod(instance->panelWindow, "hidePanel");
        }
    }
}

void PanelEngine::onLanguageChanged()
{
    if (engine) {
        engine->retranslate();
    }
}

void PanelEngine::setupLocalServer()
{
    localServer = new QLocalServer(this);
    QLocalServer::removeServer("QontrolPanel");

    localServer->setSocketOptions(QLocalServer::UserAccessOption);
    if (!localServer->listen("QontrolPanel")) {
        qWarning() << "Failed to create local server:" << localServer->errorString();
        QTimer::singleShot(0, qApp, [] { QCoreApplication::exit(EXIT_FAILURE); });
        return;
    }

    connect(localServer, &QLocalServer::newConnection,
            this, &PanelEngine::onNewConnection);
}

void PanelEngine::cleanupLocalServer()
{
    if (localServer) {
        localServer->close();
        QLocalServer::removeServer("QontrolPanel");
        delete localServer;
        localServer = nullptr;
    }
}

void PanelEngine::onNewConnection()
{
    QLocalSocket* clientSocket = localServer->nextPendingConnection();
    if (!clientSocket) {
        return;
    }

    QTimer::singleShot(3000, clientSocket, &QLocalSocket::abort);
    clientSocket->setReadBufferSize(64);
    connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
        QByteArray data = clientSocket->property("requestBytes").toByteArray() + clientSocket->readAll();
        if (data.size() > 32)
        {
            clientSocket->abort();
            return;
        }
        clientSocket->setProperty("requestBytes", data);
        if (!data.contains('\n') && data != "show_panel")
            return;
        const QString message = QString::fromUtf8(data).trimmed();

        if (message == "show_panel")
        {
            if (panelWindow)
            {
                QMetaObject::invokeMethod(panelWindow, "showPanel");
            }
        }

        clientSocket->disconnectFromServer();
    });

    connect(clientSocket, &QLocalSocket::disconnected,
            this, &PanelEngine::onClientDisconnected);
}

void PanelEngine::onClientDisconnected()
{
    QLocalSocket* clientSocket = qobject_cast<QLocalSocket*>(sender());
    if (clientSocket) {
        clientSocket->deleteLater();
    }
}
