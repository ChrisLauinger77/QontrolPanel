#pragma once
#include <QObject>
#include <QQmlEngine>
#include <QAbstractNativeEventFilter>
#include <QMap>
#include <QJsonArray>
#include <QJsonObject>
#include <Windows.h>

struct AppVolumeHotkey {
    QString executableName;
    int volumeUpKey = 0;
    int volumeUpModifiers = 0;
    int volumeDownKey = 0;
    int volumeDownModifiers = 0;
    int volumeStepSize = 0;
    int volumeUpHotkeyId = 0;
    int volumeDownHotkeyId = 0;
};

class KeyboardShortcutManager : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool globalShortcutsSuspended READ globalShortcutsSuspended WRITE setGlobalShortcutsSuspended NOTIFY globalShortcutsSuspendedChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QJsonArray appVolumeHotkeys READ appVolumeHotkeysJson NOTIFY appVolumeHotkeysChanged)

public:
    static KeyboardShortcutManager* instance();
    static KeyboardShortcutManager* create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    ~KeyboardShortcutManager() override;

    QString lastError() const { return m_lastError; }
    bool globalShortcutsSuspended() const;
    void setGlobalShortcutsSuspended(bool suspended);

    Q_INVOKABLE void manageGlobalShortcuts(bool enabled);

    // Per-app volume hotkeys
    Q_INVOKABLE bool addAppVolumeHotkey(const QString &executableName, int volUpKey, int volUpMods, int volDownKey, int volDownMods, int volumeStepSize = 0);
    Q_INVOKABLE void removeAppVolumeHotkey(const QString &executableName);
    QJsonArray appVolumeHotkeysJson() const;

    // Native event filter to handle WM_HOTKEY messages
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void lastErrorChanged();
    void registrationFailed(const QString& message);
    void panelToggleRequested();
    void globalShortcutsSuspendedChanged();
    void chatMixEnabledChanged(bool enabled);
    void chatMixNotificationRequested(QString message);
    void chatMixToggleRequested();
    void micMuteToggleRequested();
    void appVolumeHotkeyPressed(const QString& executableName, bool volumeUp, int volumeStepSize);
    void appVolumeHotkeysChanged();

private:
    explicit KeyboardShortcutManager(QObject *parent = nullptr);
    static KeyboardShortcutManager* m_instance;

    enum HotkeyId {
        HOTKEY_PANEL_TOGGLE = 1,
        HOTKEY_CHATMIX_TOGGLE = 2,
        HOTKEY_MIC_MUTE = 3
    };

    static const int APP_HOTKEY_BASE_ID = 100;

    int qtKeyToVirtualKey(int qtKey);
    UINT convertQtModifiers(int qtMods);
    bool m_globalShortcutsSuspended = false;

    HWND m_hwnd = nullptr;
    QMap<int, bool> m_registeredHotkeys;
    QMap<int, QPair<UINT, UINT>> m_bindings;
    QString m_lastError;
    bool m_registering = false;
    bool m_registrationQueued = false;
    QMap<int, QPair<int, int>> m_acceptedGlobalBindings;
    bool registerBinding(int id, UINT modifiers, UINT key);

    // Per-app volume hotkeys
    QList<AppVolumeHotkey> m_appVolumeHotkeys;
    int m_nextAppHotkeyId = APP_HOTKEY_BASE_ID;

    void registerHotkeys();
    void unregisterHotkeys();

    void registerAppVolumeHotkeys();
    void unregisterAppVolumeHotkeys();
    void loadAppVolumeHotkeys();
    void saveAppVolumeHotkeys();
    QString getAppVolumeHotkeysFilePath() const;
};
