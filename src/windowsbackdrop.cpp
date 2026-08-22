#include "windowsbackdrop.h"

#include "logmanager.h"

#include <QGuiApplication>
#include <QPointer>
#include <QSettings>
#include <QStyleHints>
#include <QWindow>

#include <windows.h>
#include <dwmapi.h>

#include <memory>
#include <unordered_map>

namespace {
constexpr char LogCategory[] = "WindowsBackdrop";

enum class BackdropKind
{
    Transient,
    MainWindow,
};

QString formatHresult(HRESULT result)
{
    return QStringLiteral("0x%1")
        .arg(static_cast<qulonglong>(static_cast<ULONG>(result)), 8, 16, QLatin1Char('0'));
}

QWindow* windowFromObject(QObject* windowObject)
{
    return qobject_cast<QWindow*>(windowObject);
}

bool usesDarkTheme()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

bool transparencyEffectsEnabled()
{
    HIGHCONTRASTW highContrast{sizeof(HIGHCONTRASTW)};
    const BOOL highContrastStateAvailable = SystemParametersInfoW(
        SPI_GETHIGHCONTRAST,
        sizeof(HIGHCONTRASTW),
        &highContrast,
        0);
    if (!highContrastStateAvailable
        || (highContrast.dwFlags & HCF_HIGHCONTRASTON) != 0) {
        return false;
    }

    BOOL compositionEnabled = FALSE;
    if (FAILED(DwmIsCompositionEnabled(&compositionEnabled)) || !compositionEnabled) {
        return false;
    }

    QSettings personalizeSettings(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    if (personalizeSettings.value(QStringLiteral("EnableTransparency"), 1).toInt() == 0) {
        return false;
    }
    return true;
}

struct AccentPolicy
{
    DWORD state;
    DWORD flags;
    DWORD gradientColor;
    DWORD animationId;
};

struct CompositionAttributeData
{
    DWORD attribute;
    void* data;
    SIZE_T dataSize;
};

using SetWindowCompositionAttributeFunction =
    BOOL(WINAPI*)(HWND, const CompositionAttributeData*);

bool applyWindowAcrylic(HWND hwnd, bool enabled)
{
    // This compatibility API has no import library or public structure
    // declarations. Resolve it dynamically so unsupported systems retain the
    // documented DWM transient-backdrop fallback.
    static const auto setWindowCompositionAttribute =
        reinterpret_cast<SetWindowCompositionAttributeFunction>(GetProcAddress(
            GetModuleHandleW(L"user32.dll"),
            "SetWindowCompositionAttribute"));
    if (!setWindowCompositionAttribute) {
        return false;
    }

    constexpr DWORD accentDisabled = 0;
    constexpr DWORD accentEnableAcrylicBlurBehind = 4;
    constexpr DWORD accentPolicyAttribute = 19;
    constexpr DWORD useGradientColor = 2;
    // AABBGGRR: retain a luminosity layer while allowing the live desktop hue
    // behind the popup to remain visible.
    constexpr DWORD darkAcrylicGradient = 0x99202020;
    constexpr DWORD lightAcrylicGradient = 0x99F3F3F3;

    AccentPolicy policy{
        enabled ? accentEnableAcrylicBlurBehind : accentDisabled,
        enabled ? useGradientColor : 0,
        enabled ? (usesDarkTheme() ? darkAcrylicGradient : lightAcrylicGradient) : 0,
        0,
    };
    const CompositionAttributeData attributeData{
        accentPolicyAttribute,
        &policy,
        sizeof(policy),
    };
    return setWindowCompositionAttribute(hwnd, &attributeData) != FALSE;
}

bool applyDwmBackdrop(HWND hwnd, DWM_SYSTEMBACKDROP_TYPE backdropType)
{
    const HRESULT result = DwmSetWindowAttribute(
        hwnd,
        DWMWA_SYSTEMBACKDROP_TYPE,
        &backdropType,
        sizeof(backdropType));
    if (FAILED(result)) {
        LOG_WARN(LogCategory,
                 QString("Failed to apply system backdrop: %1")
                     .arg(formatHresult(result)));
        return false;
    }
    return true;
}

bool setRedirectionBitmapAlpha(HWND hwnd, bool enabled)
{
    const BOOL useAlpha = enabled;
    const HRESULT result = DwmSetWindowAttribute(
        hwnd,
        DWMWA_REDIRECTIONBITMAP_ALPHA,
        &useAlpha,
        sizeof(useAlpha));
    if (FAILED(result)) {
        LOG_WARN(LogCategory,
                 QString("Failed to update client-area alpha composition: %1")
                     .arg(formatHresult(result)));
        return false;
    }
    return true;
}

void clearDwmBackdrop(HWND hwnd)
{
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

void applyCommonDwmAttributes(HWND hwnd)
{
    const BOOL useDarkMode = usesDarkTheme();
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
}

bool applyMaterial(QWindow* window, BackdropKind kind)
{
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return false;
    }

    applyCommonDwmAttributes(hwnd);

    if (kind == BackdropKind::MainWindow) {
        if (!transparencyEffectsEnabled()) {
            applyWindowAcrylic(hwnd, false);
            clearDwmBackdrop(hwnd);
            setRedirectionBitmapAlpha(hwnd, false);
            return false;
        }
        if (!setRedirectionBitmapAlpha(hwnd, true)) {
            applyWindowAcrylic(hwnd, false);
            clearDwmBackdrop(hwnd);
            return false;
        }
        applyWindowAcrylic(hwnd, false);
        if (applyDwmBackdrop(hwnd, DWMSBT_MAINWINDOW)) {
            return true;
        }
        setRedirectionBitmapAlpha(hwnd, false);
        return false;
    }

    if (applyWindowAcrylic(hwnd, true)) {
        return true;
    }
    return applyDwmBackdrop(hwnd, DWMSBT_TRANSIENTWINDOW);
}

void clearMaterial(QWindow* window, BackdropKind kind)
{
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    applyWindowAcrylic(hwnd, false);
    clearDwmBackdrop(hwnd);
    if (kind == BackdropKind::MainWindow) {
        setRedirectionBitmapAlpha(hwnd, false);
    }
}
}

struct WindowsBackdrop::Impl
{
    struct TrackedWindow
    {
        QPointer<QWindow> window;
        BackdropKind kind = BackdropKind::Transient;
        QMetaObject::Connection visibleConnection;
        QMetaObject::Connection destroyedConnection;
    };

    std::unordered_map<QWindow*, std::unique_ptr<TrackedWindow>> windows;
    QMetaObject::Connection themeConnection;

    void remove(QWindow* window, bool resetMaterial)
    {
        auto iterator = windows.find(window);
        if (iterator == windows.end()) {
            return;
        }

        QObject::disconnect(iterator->second->visibleConnection);
        QObject::disconnect(iterator->second->destroyedConnection);
        if (resetMaterial && iterator->second->window) {
            clearMaterial(iterator->second->window, iterator->second->kind);
        }
        windows.erase(iterator);
    }
};

WindowsBackdrop* WindowsBackdrop::m_instance = nullptr;

WindowsBackdrop::WindowsBackdrop(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    m_impl->themeConnection = connect(
        QGuiApplication::styleHints(),
        &QStyleHints::colorSchemeChanged,
        this,
        [this]() {
            for (const auto& [window, trackedWindow] : m_impl->windows) {
                Q_UNUSED(window)
                if (trackedWindow->window) {
                    applyMaterial(trackedWindow->window, trackedWindow->kind);
                }
            }
        });
}

WindowsBackdrop::~WindowsBackdrop()
{
    QObject::disconnect(m_impl->themeConnection);
    while (!m_impl->windows.empty()) {
        m_impl->remove(m_impl->windows.begin()->first, true);
    }
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
    return applyBackdrop(windowObject, false);
}

bool WindowsBackdrop::applyMainWindowBackdrop(QObject* windowObject)
{
    return applyBackdrop(windowObject, true);
}

bool WindowsBackdrop::applyBackdrop(QObject* windowObject, bool mainWindow)
{
    const BackdropKind kind =
        mainWindow ? BackdropKind::MainWindow : BackdropKind::Transient;
    QWindow* window = windowFromObject(windowObject);
    if (!window) {
        LOG_WARN(LogCategory, "Cannot apply backdrop: object is not a window");
        return false;
    }
    const auto existingWindow = m_impl->windows.find(window);
    if (existingWindow != m_impl->windows.end()
        && existingWindow->second->kind != kind) {
        clearMaterial(window, existingWindow->second->kind);
    }
    if (!applyMaterial(window, kind)) {
        LOG_WARN(LogCategory, "Cannot apply backdrop: native material is unavailable");
        return false;
    }
    if (existingWindow != m_impl->windows.end()) {
        existingWindow->second->kind = kind;
        return true;
    }

    auto trackedWindow = std::make_unique<Impl::TrackedWindow>();
    trackedWindow->window = window;
    trackedWindow->kind = kind;
    trackedWindow->visibleConnection = connect(
        window,
        &QWindow::visibleChanged,
        this,
        [this, window](bool visible) {
            if (visible) {
                const auto trackedWindow = m_impl->windows.find(window);
                if (trackedWindow != m_impl->windows.end()) {
                    applyMaterial(window, trackedWindow->second->kind);
                }
            }
        });
    trackedWindow->destroyedConnection = connect(
        window,
        &QObject::destroyed,
        this,
        [this, window]() { m_impl->remove(window, false); });
    m_impl->windows.emplace(window, std::move(trackedWindow));

    LOG_INFO(LogCategory,
             kind == BackdropKind::MainWindow
                 ? "Applied activation-aware Windows Mica material"
                 : "Applied dispatcher-free Windows acrylic material");
    return true;
}

void WindowsBackdrop::removeBackdrop(QObject* windowObject)
{
    QWindow* window = windowFromObject(windowObject);
    if (!window) {
        return;
    }
    m_impl->remove(window, true);
}
