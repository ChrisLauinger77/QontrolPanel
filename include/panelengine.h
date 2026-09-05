#pragma once

#include <QMainWindow>
#include <QQmlApplicationEngine>
#include <QWindow>
#include <QPointer>
#include <Windows.h>
#include <QLocalServer>
#include <QLocalSocket>

class PanelEngine : public QWidget
{
    Q_OBJECT

public:
    PanelEngine(QWidget *parent = nullptr);
    ~PanelEngine();
    static PanelEngine* instance;

private slots:
    void onLanguageChanged();
    void onNewConnection();
    void onClientDisconnected();
    void onPanelVisibilityChanged(bool visible);

private:
    QQmlApplicationEngine* engine;
    QPointer<QWindow> panelWindow;
    QPointer<QWindow> mediaPanelWindow;

    void initializeQMLEngine();
    void destroyQMLEngine();
    void showPanel();
    void hidePanel();
    bool isPanelWindow(HWND windowHandle) const;

    // Focus monitoring using Windows Event Hook
    void startFocusMonitoring();
    void stopFocusMonitoring();
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
                                       LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);
    static HWINEVENTHOOK focusHook;

    QLocalServer* localServer;
    void setupLocalServer();
    void cleanupLocalServer();
    bool isPanelVisible = false;
};
