#include "jsonstore.h"
#include "windowfocusmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFileInfo>
#include <QMetaObject>
#include <psapi.h>
#include "logmanager.h"

WindowFocusManager* WindowFocusManager::s_instance = nullptr;

WindowFocusManager::WindowFocusManager(QObject *parent)
    : QObject(parent)
    , m_winEventHook(nullptr)
    , m_isMonitoring(false)
{
    s_instance = this;
    loadSettings();
}

WindowFocusManager::~WindowFocusManager()
{
    stopMonitoring();
    s_instance = nullptr;
}

void WindowFocusManager::startMonitoring()
{
    if (m_isMonitoring) {
        return;
    }

    m_winEventHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL, WinEventProc, 0, 0,
                                     WINEVENT_OUTOFCONTEXT);

    if (m_winEventHook) {
        m_isMonitoring = true;
        QMetaObject::invokeMethod(
            this, [this] { onApplicationFocusChanged(getExecutableNameFromHwnd(GetForegroundWindow()), true); },
            Qt::QueuedConnection);
    } else {
        LOG_CRITICAL("WindowFocusManager",
                     "Failed to start window focus monitoring");
    }
}

void WindowFocusManager::stopMonitoring()
{
    if (m_winEventHook) {
        UnhookWinEvent(m_winEventHook);
        m_winEventHook = nullptr;
    }
    m_isMonitoring = false;
}

void CALLBACK WindowFocusManager::WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    Q_UNUSED(hWinEventHook)
    Q_UNUSED(idObject)
    Q_UNUSED(idChild)
    Q_UNUSED(dwEventThread)
    Q_UNUSED(dwmsEventTime)

    if (!s_instance || event != EVENT_SYSTEM_FOREGROUND || !hwnd) {
        return;
    }

    QString executableName = s_instance->getExecutableNameFromHwnd(hwnd);

    // Emit signal asynchronously to main thread
    QMetaObject::invokeMethod(s_instance, "onApplicationFocusChanged", Qt::QueuedConnection,
                              Q_ARG(QString, executableName), Q_ARG(bool, true));
}

QString WindowFocusManager::getExecutableNameFromHwnd(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return getExecutableNameFromPid(pid);
}

QString WindowFocusManager::getExecutableNameFromPid(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t path[32768]{};
    DWORD length = 32768;
    const bool success = QueryFullProcessImageNameW(process, 0, path, &length);
    CloseHandle(process);
    return success ? QFileInfo(QString::fromWCharArray(path, length)).completeBaseName().toCaseFolded() : QString{};
}

void WindowFocusManager::onApplicationFocusChanged(const QString& executableName, bool hasFocus)
{
    if (!hasFocus)
        return;
    const QString key = executableName.toCaseFolded();
    if (m_currentFocusedApp == key)
        return;
    const QString previous = m_currentFocusedApp;
    m_currentFocusedApp = key;
    if (!previous.isEmpty())
        emit applicationFocusChanged(previous, false);
    emit applicationFocusChanged(key, true);
}

bool WindowFocusManager::isApplicationMutedInBackground(const QString& executableName) const
{
    return m_backgroundMutedApps.contains(executableName.toCaseFolded());
}

bool WindowFocusManager::setApplicationMutedInBackground(const QString& executableName, bool muted)
{
    if (isApplicationMutedInBackground(executableName) == muted)
        return true;
    auto candidate = m_backgroundMutedApps;
    if (muted)
        candidate.insert(executableName.toCaseFolded());
    else
        candidate.remove(executableName.toCaseFolded());
    if (!saveSettings(candidate))
        return false;
    m_backgroundMutedApps = candidate;
    return true;
}

QStringList WindowFocusManager::getBackgroundMutedApplications() const
{
    return m_backgroundMutedApps.values();
}

void WindowFocusManager::loadSettings()
{
    QString filePath = getSettingsFilePath();
    const QJsonDocument doc = JsonStore::load(filePath, "backgroundMutedApps");
    if (doc.isNull())
        return;

    QJsonObject root = doc.object();
    QJsonArray mutedAppsArray = root["backgroundMutedApps"].toArray();

    m_backgroundMutedApps.clear();
    for (const QJsonValue& value : mutedAppsArray) {
        m_backgroundMutedApps.insert(value.toString().toCaseFolded());
    }
}

bool WindowFocusManager::saveSettings(const QSet<QString>& applications)
{
    QString filePath = getSettingsFilePath();

    QJsonArray mutedAppsArray;
    for (const QString& app : applications) {
        mutedAppsArray.append(app);
    }

    QJsonObject root;
    root["backgroundMutedApps"] = mutedAppsArray;

    QJsonDocument doc(root);
    return JsonStore::save(filePath, doc);
}

QString WindowFocusManager::getSettingsFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/backgroundmute.json";
}
