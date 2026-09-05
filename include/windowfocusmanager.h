#pragma once

#include <QObject>
#include <QMap>
#include <QSet>
#include <QString>
#include <Windows.h>

class WindowFocusManager : public QObject
{
    Q_OBJECT

public:
    explicit WindowFocusManager(QObject *parent = nullptr);
    ~WindowFocusManager();

    void startMonitoring();
    void stopMonitoring();

    bool isApplicationMutedInBackground(const QString& executableName) const;
    bool setApplicationMutedInBackground(const QString& executableName, bool muted);

    QStringList getBackgroundMutedApplications() const;
    bool isFocused(const QString& executable) const { return m_currentFocusedApp == executable.toCaseFolded(); }

signals:
    void applicationFocusChanged(const QString& executableName, bool hasFocus);

private slots:
    void onApplicationFocusChanged(const QString& executableName, bool hasFocus);

private:
    static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);
    static WindowFocusManager* s_instance;

    QString getExecutableNameFromHwnd(HWND hwnd);
    QString getExecutableNameFromPid(DWORD pid);

    void loadSettings();
    bool saveSettings(const QSet<QString>& applications);
    QString getSettingsFilePath() const;

    HWINEVENTHOOK m_winEventHook;
    QSet<QString> m_backgroundMutedApps;
    QString m_currentFocusedApp;
    bool m_isMonitoring;
};
