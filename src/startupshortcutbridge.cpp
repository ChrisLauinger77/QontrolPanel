#include "startupshortcutbridge.h"
#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include "logmanager.h"

StartupShortcutBridge* StartupShortcutBridge::m_instance = nullptr;

StartupShortcutBridge::StartupShortcutBridge(QObject* parent)
    : QObject(parent)
{

}

StartupShortcutBridge::~StartupShortcutBridge()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

StartupShortcutBridge* StartupShortcutBridge::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!m_instance) {
        m_instance = new StartupShortcutBridge();
    }
    return m_instance;
}

StartupShortcutBridge* StartupShortcutBridge::instance()
{
    return m_instance;
}

void StartupShortcutBridge::manageShortcut(bool state, QString shortcutName)
{
    QSettings registry("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);

    if (state) {
        QString applicationPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        registry.setValue(shortcutName, QString("\"%1\"").arg(applicationPath));
    } else {
        registry.remove(shortcutName);
    }
    registry.sync();
    if (registry.status() != QSettings::NoError)
        LOG_WARN("Startup", "Could not update startup registration");
}

bool StartupShortcutBridge::isShortcutPresent(QString shortcutName)
{
    QSettings registry("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);

    return registry.contains(shortcutName);
}

bool StartupShortcutBridge::getShortcutState()
{
    return isShortcutPresent("QontrolPanel.lnk");
}

void StartupShortcutBridge::setStartupShortcut(bool enabled)
{
    manageShortcut(enabled, "QontrolPanel.lnk");
}
