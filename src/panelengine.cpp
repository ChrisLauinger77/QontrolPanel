#include "panelengine.h"
#include "mediasessionmanager.h"
#include "LanguageBridge.h"
#include "trayiconprovider.h"
#include "usersettings.h"
#include <QMenu>
#include <QApplication>
#include <QScreen>
#include <QRect>
#include <QWindow>
#include <QQmlContext>
#include <QTimer>
#include <QFontMetrics>
#include <Windows.h>
#include <QProcess>

HWINEVENTHOOK PanelEngine::focusHook = NULL;
PanelEngine* PanelEngine::instance = nullptr;

PanelEngine::PanelEngine(QWidget *parent)
    : QWidget(parent)
    , engine(nullptr)
    , panelWindow(nullptr)
    , localServer(nullptr)
{
    UserSettings::instance();
    bool enableMediaSessionManager = UserSettings::instance()->enableMediaSessionManager();
    if (enableMediaSessionManager) {
        MediaSessionManager::initialize();
    }

    instance = this;
    initializeQMLEngine();
    setupLocalServer();

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
    cleanupLocalServer();
    instance = nullptr;
}

void PanelEngine::initializeQMLEngine()
{
    if (engine) {
        return;
    }

    engine = new QQmlApplicationEngine(this);
    engine->addImageProvider("trayicon", new TrayIconProvider());
    engine->loadFromModule("ChrisLauinger77.QontrolPanel", "Main");

    if (!engine->rootObjects().isEmpty()) {
        panelWindow = qobject_cast<QWindow*>(engine->rootObjects().constFirst());
        if (panelWindow) {
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
    } else {
        stopFocusMonitoring();
    }
}

void PanelEngine::destroyQMLEngine()
{
    if (engine) {
        engine->deleteLater();
        engine = nullptr;
    }
    panelWindow = nullptr;
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
        HWND panelWindow = (HWND)instance->panelWindow->winId();

        if (hwnd != panelWindow) {
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

    if (!localServer->listen("QontrolPanel")) {
        qWarning() << "Failed to create local server:" << localServer->errorString();
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

    connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
        QByteArray data = clientSocket->readAll();
        QString message = QString::fromUtf8(data);

        if (message == "show_panel") {
            if (panelWindow) {
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
