#include "windowsbackdrop.h"

#include "logmanager.h"

#include <QDebug>
#include <QGuiApplication>
#include <QPointer>
#include <QStyleHints>
#include <QWindow>

#include <windows.h>
#include <dispatcherqueue.h>
#include <dwmapi.h>

#include <MddBootstrap.h>
#include <WindowsAppSDK-VersionInfo.h>
#include <Windows.UI.Composition.Interop.h>

#include <winrt/Microsoft.UI.Composition.SystemBackdrops.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.h>

#include <memory>
#include <unordered_map>

namespace {
constexpr char LogCategory[] = "WindowsBackdrop";

bool runtimeInitialized = false;
bool apartmentInitialized = false;

QString formatHresult(HRESULT result)
{
    return QStringLiteral("0x%1")
        .arg(static_cast<qulonglong>(static_cast<ULONG>(result)), 8, 16, QLatin1Char('0'));
}

QWindow* windowFromObject(QObject* windowObject)
{
    return qobject_cast<QWindow*>(windowObject);
}

winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropTheme currentTheme()
{
    using Theme = winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropTheme;

    switch (QGuiApplication::styleHints()->colorScheme()) {
    case Qt::ColorScheme::Dark:
        return Theme::Dark;
    case Qt::ColorScheme::Light:
        return Theme::Light;
    default:
        return Theme::Default;
    }
}

bool usesDarkTheme()
{
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}

winrt::Windows::UI::Color color(BYTE red, BYTE green, BYTE blue)
{
    return winrt::Windows::UI::Color{255, red, green, blue};
}

bool applyDwmFallback(HWND hwnd)
{
    const DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_TRANSIENTWINDOW;
    const HRESULT result = DwmSetWindowAttribute(
        hwnd,
        DWMWA_SYSTEMBACKDROP_TYPE,
        &backdropType,
        sizeof(backdropType));
    if (FAILED(result)) {
        LOG_WARN(LogCategory,
                 QString("Failed to apply fallback transient backdrop: %1")
                     .arg(formatHresult(result)));
        return false;
    }

    return true;
}

void applyCommonDwmAttributes(HWND hwnd)
{
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
}

struct WindowsBackdrop::Impl
{
    using AcrylicController =
        winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController;
    using BackdropConfiguration =
        winrt::Microsoft::UI::Composition::SystemBackdrops::SystemBackdropConfiguration;
    using DesktopWindowTarget =
        winrt::Windows::UI::Composition::Desktop::DesktopWindowTarget;

    struct WindowBackdrop
    {
        QPointer<QWindow> window;
        DesktopWindowTarget target{nullptr};
        AcrylicController controller{nullptr};
        BackdropConfiguration configuration{nullptr};
        QMetaObject::Connection visibleConnection;
        QMetaObject::Connection destroyedConnection;
    };

    winrt::Windows::System::DispatcherQueue dispatcherQueue{nullptr};
    winrt::Windows::System::DispatcherQueueController dispatcherController{nullptr};
    winrt::Windows::UI::Composition::Compositor compositor{nullptr};
    std::unordered_map<QWindow*, std::unique_ptr<WindowBackdrop>> backdrops;
    QMetaObject::Connection themeConnection;

    bool ensureCompositionInfrastructure()
    {
        if (!runtimeInitialized || !AcrylicController::IsSupported()) {
            return false;
        }

        try {
            if (!dispatcherQueue) {
                dispatcherQueue =
                    winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
            }
            if (!dispatcherQueue) {
                const DispatcherQueueOptions options{
                    sizeof(DispatcherQueueOptions),
                    DQTYPE_THREAD_CURRENT,
                    DQTAT_COM_NONE,
                };
                ABI::Windows::System::IDispatcherQueueController* controller = nullptr;
                winrt::check_hresult(CreateDispatcherQueueController(options, &controller));
                dispatcherController = {
                    controller,
                    winrt::take_ownership_from_abi,
                };
                dispatcherQueue = dispatcherController.DispatcherQueue();
                LOG_INFO(LogCategory, "Created Windows.System DispatcherQueue on the Qt UI thread");
            }
            if (!compositor) {
                compositor = winrt::Windows::UI::Composition::Compositor();
            }
            return true;
        } catch (const winrt::hresult_error& error) {
            LOG_WARN(LogCategory,
                     QString("Failed to initialize Windows App SDK composition: %1")
                         .arg(formatHresult(error.code())));
            return false;
        }
    }

    void updateConfiguration(WindowBackdrop& backdrop)
    {
        if (!backdrop.configuration || !backdrop.window) {
            return;
        }

        // These tray surfaces intentionally avoid activation. Always report them
        // as input-active, including during controller creation while hidden, so
        // Windows uses the live acrylic recipe rather than inactive fallback grey.
        backdrop.configuration.IsInputActive(true);
        backdrop.configuration.Theme(currentTheme());

        // Keep the tint transparent so the hue comes from the wallpaper/window
        // behind the flyout. These values follow Microsoft's PowerToys thin
        // acrylic recipe; the slightly stronger dark luminosity layer preserves
        // text contrast without replacing the sampled backdrop with neutral grey.
        if (backdrop.controller) {
            const bool dark = usesDarkTheme();
            const auto neutralColor = dark ? color(32, 32, 32) : color(243, 243, 243);
            backdrop.controller.TintColor(neutralColor);
            backdrop.controller.TintOpacity(0.0f);
            backdrop.controller.LuminosityOpacity(dark ? 0.91f : 0.85f);
            backdrop.controller.FallbackColor(neutralColor);
        }
    }

    void updateAllConfigurations()
    {
        for (const auto& [window, backdrop] : backdrops) {
            Q_UNUSED(window)
            updateConfiguration(*backdrop);
        }
    }

    void remove(QWindow* window)
    {
        auto iterator = backdrops.find(window);
        if (iterator == backdrops.end()) {
            return;
        }

        QObject::disconnect(iterator->second->visibleConnection);
        QObject::disconnect(iterator->second->destroyedConnection);
        if (iterator->second->controller) {
            iterator->second->controller.Close();
        }
        backdrops.erase(iterator);
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
        [this]() { m_impl->updateAllConfigurations(); });
}

WindowsBackdrop::~WindowsBackdrop()
{
    QObject::disconnect(m_impl->themeConnection);
    while (!m_impl->backdrops.empty()) {
        m_impl->remove(m_impl->backdrops.begin()->first);
    }
    m_impl->compositor = nullptr;
    m_impl->dispatcherQueue = nullptr;
    if (m_impl->dispatcherController) {
        try {
            const auto shutdown = m_impl->dispatcherController.ShutdownQueueAsync();
            while (shutdown.Status() == winrt::Windows::Foundation::AsyncStatus::Started) {
                MSG message;
                if (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&message);
                    DispatchMessage(&message);
                } else {
                    MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
                }
            }
            shutdown.GetResults();
        } catch (const winrt::hresult_error& error) {
            LOG_WARN(LogCategory,
                     QString("Failed to shut down the composition dispatcher queue: %1")
                         .arg(formatHresult(error.code())));
        }
        m_impl->dispatcherController = nullptr;
    }

    if (m_instance == this) {
        m_instance = nullptr;
    }
}

bool WindowsBackdrop::initializeRuntime()
{
    if (runtimeInitialized) {
        return true;
    }

    const PACKAGE_VERSION minimumVersion{};
    const HRESULT bootstrapResult = MddBootstrapInitialize(
        WINDOWSAPPSDK_RELEASE_MAJORMINOR,
        WINDOWSAPPSDK_RELEASE_VERSION_TAG_W,
        minimumVersion);
    if (FAILED(bootstrapResult)) {
        qWarning().noquote()
            << QString("Windows App SDK runtime initialization failed: %1")
                   .arg(formatHresult(bootstrapResult));
        return false;
    }

    try {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        apartmentInitialized = true;
    } catch (const winrt::hresult_error& error) {
        qWarning().noquote()
            << QString("Windows Runtime apartment initialization failed: %1")
                   .arg(formatHresult(error.code()));
        MddBootstrapShutdown();
        return false;
    }

    runtimeInitialized = true;
    return true;
}

void WindowsBackdrop::shutdownRuntime()
{
    if (m_instance) {
        delete m_instance;
    }

    if (apartmentInitialized) {
        winrt::uninit_apartment();
        apartmentInitialized = false;
    }
    if (runtimeInitialized) {
        MddBootstrapShutdown();
        runtimeInitialized = false;
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

    applyCommonDwmAttributes(hwnd);

    if (const auto existing = m_impl->backdrops.find(window);
        existing != m_impl->backdrops.end()) {
        m_impl->updateConfiguration(*existing->second);
        return true;
    }

    if (!m_impl->ensureCompositionInfrastructure()) {
        return applyDwmFallback(hwnd);
    }

    try {
        const BOOL useHostBackdropBrush = TRUE;
        winrt::check_hresult(DwmSetWindowAttribute(
            hwnd,
            DWMWA_USE_HOSTBACKDROPBRUSH,
            &useHostBackdropBrush,
            sizeof(useHostBackdropBrush)));

        auto compositorInterop =
            m_impl->compositor.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
        Impl::DesktopWindowTarget target{nullptr};
        winrt::check_hresult(compositorInterop->CreateDesktopWindowTarget(
            hwnd,
            true,
            reinterpret_cast<ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget**>(
                winrt::put_abi(target))));

        auto backdrop = std::make_unique<Impl::WindowBackdrop>();
        backdrop->window = window;
        backdrop->target = target;
        backdrop->configuration = Impl::BackdropConfiguration();
        backdrop->controller = Impl::AcrylicController();
        backdrop->controller.Kind(
            winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicKind::Thin);
        m_impl->updateConfiguration(*backdrop);
        backdrop->controller.SetSystemBackdropConfiguration(backdrop->configuration);

        const auto windowId = winrt::Microsoft::UI::GetWindowIdFromWindow(hwnd);
        if (!backdrop->controller.SetTarget(windowId, target)) {
            LOG_WARN(LogCategory, "DesktopAcrylicController rejected the native window target");
            return applyDwmFallback(hwnd);
        }
        LOG_INFO(LogCategory,
                 QString("Desktop acrylic attached with state %1")
                     .arg(static_cast<int>(backdrop->controller.State())));

        backdrop->visibleConnection = connect(
            window,
            &QWindow::visibleChanged,
            this,
            [this, window]() {
                const auto iterator = m_impl->backdrops.find(window);
                if (iterator != m_impl->backdrops.end()) {
                    m_impl->updateConfiguration(*iterator->second);
                }
            });
        backdrop->destroyedConnection = connect(
            window,
            &QObject::destroyed,
            this,
            [this, window]() { m_impl->remove(window); });

        m_impl->backdrops.emplace(window, std::move(backdrop));
        return true;
    } catch (const winrt::hresult_error& error) {
        LOG_WARN(LogCategory,
                 QString("Failed to apply DesktopAcrylicController: %1")
                     .arg(formatHresult(error.code())));
        return applyDwmFallback(hwnd);
    }
}

void WindowsBackdrop::removeBackdrop(QObject* windowObject)
{
    QWindow* window = windowFromObject(windowObject);
    if (!window) {
        return;
    }

    m_impl->remove(window);

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        return;
    }

    const BOOL useHostBackdropBrush = FALSE;
    DwmSetWindowAttribute(
        hwnd,
        DWMWA_USE_HOSTBACKDROPBRUSH,
        &useHostBackdropBrush,
        sizeof(useHostBackdropBrush));

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
