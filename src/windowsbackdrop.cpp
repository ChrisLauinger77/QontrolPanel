#include "windowsbackdrop.h"

#include "logmanager.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QWindow>

#include <windows.h>
#include <dwmapi.h>

namespace {
constexpr char LogCategory[] = "WindowsBackdrop";

QString formatHresult(HRESULT result)
{
    return QStringLiteral("0x%1")
        .arg(static_cast<qulonglong>(static_cast<ULONG>(result)), 8, 16, QLatin1Char('0'));
}

QWindow* windowFromObject(QObject* windowObject)
{
    return qobject_cast<QWindow*>(windowObject);
}
}

WindowsBackdrop* WindowsBackdrop::m_instance = nullptr;

WindowsBackdrop::WindowsBackdrop(QObject* parent)
    : QObject(parent)
{
}

WindowsBackdrop::~WindowsBackdrop()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

WindowsBackdrop* WindowsBackdrop::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!m_instance) {
        m_instance = new WindowsBackdrop();
    }
    return m_instance;
}

WindowsBackdrop* WindowsBackdrop::instance()
{
    return m_instance;
}

bool WindowsBackdrop::applyTransientBackdrop(QObject* windowObject)
{
    QWindow* window = windowFromObject(windowObject);
    if (!window) {
        LOG_WARN(LogCategory, "Cannot apply backdrop: object is not a window");
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        LOG_WARN(LogCategory, "Cannot apply backdrop: native window handle is unavailable");
        return false;
    }

    const BOOL useDarkMode = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
    const HRESULT darkModeResult = DwmSetWindowAttribute(
        hwnd,
        DWMWA_USE_IMMERSIVE_DARK_MODE,
        &useDarkMode,
        sizeof(useDarkMode));
    if (FAILED(darkModeResult)) {
        LOG_WARN(LogCategory,
                 QString("Failed to update immersive dark mode: %1")
                     .arg(formatHresult(darkModeResult)));
    }

    const DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_TRANSIENTWINDOW;
    const HRESULT backdropResult = DwmSetWindowAttribute(
        hwnd,
        DWMWA_SYSTEMBACKDROP_TYPE,
        &backdropType,
        sizeof(backdropType));
    if (FAILED(backdropResult)) {
        LOG_WARN(LogCategory,
                 QString("Failed to apply transient system backdrop: %1")
                     .arg(formatHresult(backdropResult)));
        return false;
    }

    const DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
    const HRESULT cornerResult = DwmSetWindowAttribute(
        hwnd,
        DWMWA_WINDOW_CORNER_PREFERENCE,
        &cornerPreference,
        sizeof(cornerPreference));
    if (FAILED(cornerResult)) {
        LOG_WARN(LogCategory,
                 QString("Failed to apply rounded window corners: %1")
                     .arg(formatHresult(cornerResult)));
    }

    return true;
}

void WindowsBackdrop::removeBackdrop(QObject* windowObject)
{
    QWindow* window = windowFromObject(windowObject);
    if (!window) {
        return;
    }

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    const DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_NONE;
    const HRESULT result = DwmSetWindowAttribute(
        hwnd,
        DWMWA_SYSTEMBACKDROP_TYPE,
        &backdropType,
        sizeof(backdropType));
    if (FAILED(result)) {
        LOG_WARN(LogCategory,
                 QString("Failed to remove system backdrop: %1")
                     .arg(formatHresult(result)));
    }
}
