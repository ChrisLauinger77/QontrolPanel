#include "jsonstore.h"
#include "logmanager.h"
#include "keyboardshortcutmanager.h"
#include "usersettings.h"
#include <QCoreApplication>
#include <QWindow>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

KeyboardShortcutManager* KeyboardShortcutManager::m_instance = nullptr;

KeyboardShortcutManager* KeyboardShortcutManager::instance()
{
    if (!m_instance) {
        m_instance = new KeyboardShortcutManager();
    }
    return m_instance;
}

KeyboardShortcutManager* KeyboardShortcutManager::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

KeyboardShortcutManager::KeyboardShortcutManager(QObject *parent)
    : QObject(parent)
{
    // Get a window handle - we'll use a message-only window
    m_hwnd = CreateWindowEx(0, L"STATIC", L"QontrolPanelHotkeys",
                            0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    // Install the native event filter
    QCoreApplication::instance()->installNativeEventFilter(this);

    // Load per-app volume hotkeys from file
    loadAppVolumeHotkeys();

    if (UserSettings::instance()->globalShortcutsEnabled()) {
        registerHotkeys();
    }
}

KeyboardShortcutManager::~KeyboardShortcutManager()
{
    unregisterHotkeys();

    // Remove the native event filter
    QCoreApplication::instance()->removeNativeEventFilter(this);

    // Destroy the message-only window
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_instance == this) {
        m_instance = nullptr;
    }
}

void KeyboardShortcutManager::manageGlobalShortcuts(bool enabled)
{
    if (!enabled)
    {
        m_registrationQueued = false;
        unregisterHotkeys();
        return;
    }
    if (m_registrationQueued)
        return;
    m_registrationQueued = true;
    QTimer::singleShot(0, this, [this] {
        if (!m_registrationQueued)
            return;
        m_registrationQueued = false;
        if (UserSettings::instance()->globalShortcutsEnabled())
        registerHotkeys();
    });
}

bool KeyboardShortcutManager::globalShortcutsSuspended() const
{
    return m_globalShortcutsSuspended;
}

void KeyboardShortcutManager::setGlobalShortcutsSuspended(bool suspended)
{
    if (m_globalShortcutsSuspended == suspended)
        return;

    m_globalShortcutsSuspended = suspended;
    emit globalShortcutsSuspendedChanged();
}

void KeyboardShortcutManager::registerHotkeys()
{
    if (m_registering || !m_hwnd)
        return;
    m_registering = true;
    m_lastError.clear();
    auto* settings = UserSettings::instance();
    auto apply = [&](int id, int key, int modifiers, auto restore) {
        if (registerBinding(id, convertQtModifiers(modifiers), qtKeyToVirtualKey(key)))
        {
            m_acceptedGlobalBindings[id] = qMakePair(key, modifiers);
    }
        else if (m_acceptedGlobalBindings.contains(id))
        {
            const auto previous = m_acceptedGlobalBindings.value(id);
            restore(previous.first, previous.second);
    }
    };
    apply(HOTKEY_PANEL_TOGGLE, settings->panelShortcutKey(), settings->panelShortcutModifiers(),
          [&](int key, int modifiers) {
              settings->setPanelShortcutKey(key);
              settings->setPanelShortcutModifiers(modifiers);
          });
    apply(HOTKEY_CHATMIX_TOGGLE, settings->chatMixShortcutKey(), settings->chatMixShortcutModifiers(),
          [&](int key, int modifiers) {
              settings->setChatMixShortcutKey(key);
              settings->setChatMixShortcutModifiers(modifiers);
          });
    apply(HOTKEY_MIC_MUTE, settings->micMuteShortcutKey(), settings->micMuteShortcutModifiers(),
          [&](int key, int modifiers) {
              settings->setMicMuteShortcutKey(key);
              settings->setMicMuteShortcutModifiers(modifiers);
          });
    registerAppVolumeHotkeys();
    m_registering = false;
    emit lastErrorChanged();
}

void KeyboardShortcutManager::unregisterHotkeys()
{
    if (!m_hwnd) return;

    for (auto it = m_registeredHotkeys.begin(); it != m_registeredHotkeys.end(); ++it) {
        UnregisterHotKey(m_hwnd, it.key());
    }
    m_registeredHotkeys.clear();
    m_bindings.clear();
}

UINT KeyboardShortcutManager::convertQtModifiers(int qtMods)
{
    UINT winMods = 0;
    if (qtMods & Qt::ControlModifier) winMods |= MOD_CONTROL;
    if (qtMods & Qt::ShiftModifier) winMods |= MOD_SHIFT;
    if (qtMods & Qt::AltModifier) winMods |= MOD_ALT;
    if (qtMods & Qt::MetaModifier)
        winMods |= MOD_WIN;
    return winMods;
}

bool KeyboardShortcutManager::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(result)

    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        MSG* msg = static_cast<MSG*>(message);

        if (msg->message == WM_HOTKEY && msg->hwnd == m_hwnd)
        {
            // Check if shortcuts are suspended
            if (m_globalShortcutsSuspended) {
                return false;
            }

            int hotkeyId = static_cast<int>(msg->wParam);

            // Handle the hotkey based on its ID
            switch (hotkeyId) {
                case HOTKEY_PANEL_TOGGLE:
                    emit panelToggleRequested();
                    return true;
                case HOTKEY_CHATMIX_TOGGLE:
                    emit chatMixToggleRequested();
                    return true;
                case HOTKEY_MIC_MUTE:
                    emit micMuteToggleRequested();
                    return true;
            }

            // Check per-app volume hotkeys
            for (const auto &appHotkey : m_appVolumeHotkeys) {
                if (hotkeyId == appHotkey.volumeUpHotkeyId) {
                    emit appVolumeHotkeyPressed(appHotkey.executableName, true, appHotkey.volumeStepSize);
                    return true;
                }
                if (hotkeyId == appHotkey.volumeDownHotkeyId) {
                    emit appVolumeHotkeyPressed(appHotkey.executableName, false, appHotkey.volumeStepSize);
                    return true;
                }
            }
        }
    }

    return false;
}

int KeyboardShortcutManager::qtKeyToVirtualKey(int qtKey)
{
    switch (qtKey) {
    case Qt::Key_A: return 0x41;
    case Qt::Key_B: return 0x42;
    case Qt::Key_C: return 0x43;
    case Qt::Key_D: return 0x44;
    case Qt::Key_E: return 0x45;
    case Qt::Key_F: return 0x46;
    case Qt::Key_G: return 0x47;
    case Qt::Key_H: return 0x48;
    case Qt::Key_I: return 0x49;
    case Qt::Key_J: return 0x4A;
    case Qt::Key_K: return 0x4B;
    case Qt::Key_L: return 0x4C;
    case Qt::Key_M: return 0x4D;
    case Qt::Key_N: return 0x4E;
    case Qt::Key_O: return 0x4F;
    case Qt::Key_P: return 0x50;
    case Qt::Key_Q: return 0x51;
    case Qt::Key_R: return 0x52;
    case Qt::Key_S: return 0x53;
    case Qt::Key_T: return 0x54;
    case Qt::Key_U: return 0x55;
    case Qt::Key_V: return 0x56;
    case Qt::Key_W: return 0x57;
    case Qt::Key_X: return 0x58;
    case Qt::Key_Y: return 0x59;
    case Qt::Key_Z: return 0x5A;
    case Qt::Key_F1: return VK_F1;
    case Qt::Key_F2: return VK_F2;
    case Qt::Key_F3: return VK_F3;
    case Qt::Key_F4: return VK_F4;
    case Qt::Key_F5: return VK_F5;
    case Qt::Key_F6: return VK_F6;
    case Qt::Key_F7: return VK_F7;
    case Qt::Key_F8: return VK_F8;
    case Qt::Key_F9: return VK_F9;
    case Qt::Key_F10: return VK_F10;
    case Qt::Key_F11: return VK_F11;
    case Qt::Key_F12: return VK_F12;
    case Qt::Key_F13: return VK_F13;
    case Qt::Key_F14: return VK_F14;
    case Qt::Key_F15: return VK_F15;
    case Qt::Key_F16: return VK_F16;
    case Qt::Key_F17: return VK_F17;
    case Qt::Key_F18: return VK_F18;
    case Qt::Key_F19: return VK_F19;
    case Qt::Key_F20: return VK_F20;
    case Qt::Key_F21: return VK_F21;
    case Qt::Key_F22: return VK_F22;
    case Qt::Key_F23: return VK_F23;
    case Qt::Key_F24: return VK_F24;
    case Qt::Key_0: return 0x30;
    case Qt::Key_1: return 0x31;
    case Qt::Key_2: return 0x32;
    case Qt::Key_3: return 0x33;
    case Qt::Key_4: return 0x34;
    case Qt::Key_5: return 0x35;
    case Qt::Key_6: return 0x36;
    case Qt::Key_7: return 0x37;
    case Qt::Key_8: return 0x38;
    case Qt::Key_9: return 0x39;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Return: return VK_RETURN;
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    default: return 0;
    }
}

// Per-app volume hotkey methods

QString KeyboardShortcutManager::getAppVolumeHotkeysFilePath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/appvolumehotkeys.json";
}

void KeyboardShortcutManager::loadAppVolumeHotkeys()
{
    const auto doc = JsonStore::load(getAppVolumeHotkeysFilePath());
    if (!doc.isArray())
        return;

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        AppVolumeHotkey hotkey;
        hotkey.executableName = obj["executableName"].toString();
        hotkey.volumeUpKey = obj["volumeUpKey"].toInt();
        hotkey.volumeUpModifiers = obj["volumeUpModifiers"].toInt();
        hotkey.volumeDownKey = obj["volumeDownKey"].toInt();
        hotkey.volumeDownModifiers = obj["volumeDownModifiers"].toInt();
        hotkey.volumeStepSize = obj["volumeStepSize"].toInt(0);
        m_appVolumeHotkeys.append(hotkey);
    }
}

bool KeyboardShortcutManager::saveAppVolumeHotkeys(const QList<AppVolumeHotkey>& hotkeys)
{
    QJsonArray arr;
    for (const auto &hotkey : hotkeys) {
        QJsonObject obj;
        obj["executableName"] = hotkey.executableName;
        obj["volumeUpKey"] = hotkey.volumeUpKey;
        obj["volumeUpModifiers"] = hotkey.volumeUpModifiers;
        obj["volumeDownKey"] = hotkey.volumeDownKey;
        obj["volumeDownModifiers"] = hotkey.volumeDownModifiers;
        obj["volumeStepSize"] = hotkey.volumeStepSize;
        arr.append(obj);
    }

    return JsonStore::save(getAppVolumeHotkeysFilePath(), QJsonDocument(arr));
}

void KeyboardShortcutManager::reportAppVolumeHotkeysSaveFailure()
{
    LOG_WARN("Shortcuts", "Could not persist application volume hotkeys");
    m_lastError = tr("Could not save application shortcuts. Check access to your user profile and available disk space.");
    emit lastErrorChanged();
    emit saveFailed(m_lastError);
}

void KeyboardShortcutManager::registerAppVolumeHotkeys()
{
    if (!m_hwnd) return;


    for (auto &hotkey : m_appVolumeHotkeys) {
        // Register volume up
        UINT upMods = convertQtModifiers(hotkey.volumeUpModifiers);
        UINT upKey = qtKeyToVirtualKey(hotkey.volumeUpKey);
        if (!hotkey.volumeUpHotkeyId)
        hotkey.volumeUpHotkeyId = m_nextAppHotkeyId++;
        registerBinding(hotkey.volumeUpHotkeyId, upMods, upKey);

        // Register volume down
        UINT downMods = convertQtModifiers(hotkey.volumeDownModifiers);
        UINT downKey = qtKeyToVirtualKey(hotkey.volumeDownKey);
        if (!hotkey.volumeDownHotkeyId)
        hotkey.volumeDownHotkeyId = m_nextAppHotkeyId++;
        registerBinding(hotkey.volumeDownHotkeyId, downMods, downKey);
    }
}

void KeyboardShortcutManager::unregisterAppVolumeHotkeys()
{
    if (!m_hwnd) return;

    for (const auto &hotkey : m_appVolumeHotkeys) {
        if (m_registeredHotkeys.contains(hotkey.volumeUpHotkeyId)) {
            UnregisterHotKey(m_hwnd, hotkey.volumeUpHotkeyId);
            m_registeredHotkeys.remove(hotkey.volumeUpHotkeyId);
            m_bindings.remove(hotkey.volumeUpHotkeyId);
        }
        if (m_registeredHotkeys.contains(hotkey.volumeDownHotkeyId)) {
            UnregisterHotKey(m_hwnd, hotkey.volumeDownHotkeyId);
            m_registeredHotkeys.remove(hotkey.volumeDownHotkeyId);
            m_bindings.remove(hotkey.volumeDownHotkeyId);
        }
    }
}

bool KeyboardShortcutManager::addAppVolumeHotkey(const QString &executableName, int volUpKey, int volUpMods, int volDownKey, int volDownMods, int volumeStepSize)
{
    if (executableName.trimmed().isEmpty())
        return false;
    const auto previous = m_appVolumeHotkeys;
    unregisterAppVolumeHotkeys();
    m_appVolumeHotkeys.removeIf([&](const AppVolumeHotkey& item) {
        return item.executableName.compare(executableName, Qt::CaseInsensitive) == 0;
    });
    AppVolumeHotkey hotkey;
    hotkey.executableName = executableName;
    hotkey.volumeUpKey = volUpKey;
    hotkey.volumeUpModifiers = volUpMods;
    hotkey.volumeDownKey = volDownKey;
    hotkey.volumeDownModifiers = volDownMods;
    hotkey.volumeStepSize = qBound(0, volumeStepSize, 100);
    m_appVolumeHotkeys.append(hotkey);
    m_lastError.clear();
    if (UserSettings::instance()->globalShortcutsEnabled())
        registerAppVolumeHotkeys();
    const bool persistenceFailed = m_lastError.isEmpty() && !saveAppVolumeHotkeys(m_appVolumeHotkeys);
    if (!m_lastError.isEmpty() || persistenceFailed)
    {
        const QString error = m_lastError;
        unregisterAppVolumeHotkeys();
        m_appVolumeHotkeys = previous;
        if (UserSettings::instance()->globalShortcutsEnabled())
            registerAppVolumeHotkeys();
        if (persistenceFailed)
            reportAppVolumeHotkeysSaveFailure();
        else
        {
            m_lastError = error;
            emit lastErrorChanged();
        }
        return false;
    }
    emit lastErrorChanged();
    emit appVolumeHotkeysChanged();
    return true;
}

bool KeyboardShortcutManager::removeAppVolumeHotkey(const QString &executableName)
{
    for (int i = 0; i < m_appVolumeHotkeys.size(); ++i) {
        if (m_appVolumeHotkeys[i].executableName == executableName) {
            auto desired = m_appVolumeHotkeys;
            desired.removeAt(i);
            // Keep the current list and native bindings until the removal is durable.
            if (!saveAppVolumeHotkeys(desired))
            {
                reportAppVolumeHotkeysSaveFailure();
                return false;
            }
            // Unregister these specific hotkeys
            if (m_hwnd) {
                if (m_registeredHotkeys.contains(m_appVolumeHotkeys[i].volumeUpHotkeyId)) {
                    UnregisterHotKey(m_hwnd, m_appVolumeHotkeys[i].volumeUpHotkeyId);
                    m_registeredHotkeys.remove(m_appVolumeHotkeys[i].volumeUpHotkeyId);
                    m_bindings.remove(m_appVolumeHotkeys[i].volumeUpHotkeyId);
                }
                if (m_registeredHotkeys.contains(m_appVolumeHotkeys[i].volumeDownHotkeyId)) {
                    UnregisterHotKey(m_hwnd, m_appVolumeHotkeys[i].volumeDownHotkeyId);
                    m_registeredHotkeys.remove(m_appVolumeHotkeys[i].volumeDownHotkeyId);
                    m_bindings.remove(m_appVolumeHotkeys[i].volumeDownHotkeyId);
                }
            }
            m_appVolumeHotkeys = desired;
            m_lastError.clear();
            emit lastErrorChanged();
            emit appVolumeHotkeysChanged();
            return true;
        }
    }
    return false;
}

QJsonArray KeyboardShortcutManager::appVolumeHotkeysJson() const
{
    QJsonArray arr;
    for (const auto &hotkey : m_appVolumeHotkeys) {
        QJsonObject obj;
        obj["executableName"] = hotkey.executableName;
        obj["volumeUpKey"] = hotkey.volumeUpKey;
        obj["volumeUpModifiers"] = hotkey.volumeUpModifiers;
        obj["volumeDownKey"] = hotkey.volumeDownKey;
        obj["volumeDownModifiers"] = hotkey.volumeDownModifiers;
        obj["volumeStepSize"] = hotkey.volumeStepSize;
        arr.append(obj);
    }
    return arr;
}

bool KeyboardShortcutManager::registerBinding(int id, UINT modifiers, UINT key)
{
    const auto previous = m_bindings.value(id);
    const auto desired = qMakePair(modifiers, key);
    if (previous == desired && m_registeredHotkeys.contains(id))
        return true;
    if (m_registeredHotkeys.contains(id))
    {
        UnregisterHotKey(m_hwnd, id);
        m_registeredHotkeys.remove(id);
        m_bindings.remove(id);
    }
    if (!key)
        return true; // An empty key deliberately disables this action.
    if (m_hwnd && RegisterHotKey(m_hwnd, id, modifiers, key))
    {
        m_registeredHotkeys[id] = true;
        m_bindings[id] = desired;
        return true;
    }
    const DWORD error = GetLastError();
    if (previous.second && RegisterHotKey(m_hwnd, id, previous.first, previous.second))
    {
        m_registeredHotkeys[id] = true;
        m_bindings[id] = previous;
    }
    m_lastError =
        tr("Windows could not register a shortcut (error %1). The previous binding was retained when possible.")
            .arg(error);
    LOG_WARN("Shortcuts", QString("Hotkey %1 registration failed: %2").arg(id).arg(error));
    emit lastErrorChanged();
    emit registrationFailed(m_lastError);
    return false;
}
