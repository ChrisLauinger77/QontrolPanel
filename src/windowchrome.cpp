#include "windowchrome.h"

#include "logmanager.h"

#include <QCoreApplication>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

namespace {
constexpr char LogCategory[] = "WindowChrome";

QQuickWindow* windowFromObject(QObject* object)
{
    return qobject_cast<QQuickWindow*>(object);
}

QQuickItem* itemFromObject(QObject* object)
{
    return qobject_cast<QQuickItem*>(object);
}

bool itemContainsNativePoint(
    const QQuickItem* item,
    const QQuickWindow* window,
    const POINT& clientPoint)
{
    if (!item || !window || !item->isVisible() || !item->isEnabled()) {
        return false;
    }

    const qreal devicePixelRatio = window->devicePixelRatio();
    const QPointF scenePoint(
        clientPoint.x / devicePixelRatio,
        clientPoint.y / devicePixelRatio);
    return item->contains(item->mapFromScene(scenePoint));
}

int resizeBorderThickness(HWND hwnd, int metric)
{
    const UINT dpi = GetDpiForWindow(hwnd);
    return GetSystemMetricsForDpi(metric, dpi)
        + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

LRESULT resizeHitTest(HWND hwnd, const POINT& clientPoint)
{
    if (IsZoomed(hwnd)) {
        return HTNOWHERE;
    }

    RECT clientRect{};
    if (!GetClientRect(hwnd, &clientRect)) {
        return HTNOWHERE;
    }

    const int horizontalBorder = resizeBorderThickness(hwnd, SM_CXSIZEFRAME);
    const int verticalBorder = resizeBorderThickness(hwnd, SM_CYSIZEFRAME);
    const bool left = clientPoint.x < horizontalBorder;
    const bool right = clientPoint.x >= clientRect.right - horizontalBorder;
    const bool top = clientPoint.y < verticalBorder;
    const bool bottom = clientPoint.y >= clientRect.bottom - verticalBorder;

    if (top && left) {
        return HTTOPLEFT;
    }
    if (top && right) {
        return HTTOPRIGHT;
    }
    if (bottom && left) {
        return HTBOTTOMLEFT;
    }
    if (bottom && right) {
        return HTBOTTOMRIGHT;
    }
    if (left) {
        return HTLEFT;
    }
    if (right) {
        return HTRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }
    return HTNOWHERE;
}
}

struct WindowChrome::Impl
{
    struct TrackedWindow
    {
        QPointer<QQuickWindow> window;
        QPointer<QQuickItem> titleBar;
        QPointer<QQuickItem> systemMenu;
        QPointer<QQuickItem> minimizeButton;
        QPointer<QQuickItem> maximizeButton;
        QPointer<QQuickItem> closeButton;
        QMetaObject::Connection destroyedConnection;
        bool maximizeButtonTracking = false;
    };

    std::unordered_map<HWND, std::unique_ptr<TrackedWindow>> windows;
};

WindowChrome* WindowChrome::m_instance = nullptr;

WindowChrome::WindowChrome(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WindowChrome::~WindowChrome()
{
    QCoreApplication::instance()->removeNativeEventFilter(this);
    for (const auto& [hwnd, trackedWindow] : m_impl->windows) {
        Q_UNUSED(hwnd)
        QObject::disconnect(trackedWindow->destroyedConnection);
    }
    m_impl->windows.clear();
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

WindowChrome* WindowChrome::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!m_instance) {
        m_instance = new WindowChrome();
    }
    return m_instance;
}

WindowChrome* WindowChrome::instance()
{
    return m_instance;
}

bool WindowChrome::maximizeButtonHovered() const
{
    return m_maximizeButtonHovered;
}

bool WindowChrome::maximizeButtonPressed() const
{
    return m_maximizeButtonPressed;
}

bool WindowChrome::installWindowChrome(
    QObject* windowObject,
    QObject* titleBarObject,
    QObject* systemMenuObject,
    QObject* minimizeButtonObject,
    QObject* maximizeButtonObject,
    QObject* closeButtonObject)
{
    QQuickWindow* window = windowFromObject(windowObject);
    QQuickItem* titleBar = itemFromObject(titleBarObject);
    QQuickItem* systemMenu = itemFromObject(systemMenuObject);
    QQuickItem* minimizeButton = itemFromObject(minimizeButtonObject);
    QQuickItem* maximizeButton = itemFromObject(maximizeButtonObject);
    QQuickItem* closeButton = itemFromObject(closeButtonObject);
    if (!window || !titleBar || !systemMenu || !minimizeButton
        || !maximizeButton || !closeButton) {
        LOG_WARN(LogCategory, "Cannot install window chrome: invalid window item");
        return false;
    }

    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd) {
        LOG_WARN(LogCategory, "Cannot install window chrome: native window is unavailable");
        return false;
    }

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    style &= ~WS_CAPTION;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);
    SetWindowPos(
        hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
            | SWP_NOACTIVATE);

    auto existingWindow = m_impl->windows.find(hwnd);
    if (existingWindow != m_impl->windows.end()) {
        existingWindow->second->titleBar = titleBar;
        existingWindow->second->systemMenu = systemMenu;
        existingWindow->second->minimizeButton = minimizeButton;
        existingWindow->second->maximizeButton = maximizeButton;
        existingWindow->second->closeButton = closeButton;
        return true;
    }

    auto trackedWindow = std::make_unique<Impl::TrackedWindow>();
    trackedWindow->window = window;
    trackedWindow->titleBar = titleBar;
    trackedWindow->systemMenu = systemMenu;
    trackedWindow->minimizeButton = minimizeButton;
    trackedWindow->maximizeButton = maximizeButton;
    trackedWindow->closeButton = closeButton;
    trackedWindow->destroyedConnection = connect(
        window,
        &QObject::destroyed,
        this,
        [this, hwnd]() {
            m_impl->windows.erase(hwnd);
            setMaximizeButtonHovered(false);
            setMaximizeButtonPressed(false);
        });
    m_impl->windows.emplace(hwnd, std::move(trackedWindow));

    LOG_INFO(LogCategory, "Installed native Settings window chrome");
    return true;
}

void WindowChrome::removeWindowChrome(QObject* windowObject)
{
    QQuickWindow* window = windowFromObject(windowObject);
    if (!window) {
        return;
    }

    for (auto iterator = m_impl->windows.begin(); iterator != m_impl->windows.end(); ++iterator) {
        if (iterator->second->window == window) {
            QObject::disconnect(iterator->second->destroyedConnection);
            m_impl->windows.erase(iterator);
            setMaximizeButtonHovered(false);
            setMaximizeButtonPressed(false);
            return;
        }
    }
}

bool WindowChrome::nativeEventFilter(
    const QByteArray& eventType,
    void* message,
    qintptr* result)
{
    if (eventType != "windows_generic_MSG"
        && eventType != "windows_dispatcher_MSG") {
        return false;
    }

    MSG* msg = static_cast<MSG*>(message);
    const auto trackedWindowIterator = m_impl->windows.find(msg->hwnd);
    if (trackedWindowIterator == m_impl->windows.end()) {
        return false;
    }

    Impl::TrackedWindow& trackedWindow = *trackedWindowIterator->second;
    if (!trackedWindow.window) {
        return false;
    }
    const auto setResult = [result](qintptr value) {
        if (result) {
            *result = value;
        }
    };
    const auto toggleMaximized = [&trackedWindow]() {
        if (trackedWindow.window->visibility() == QWindow::Maximized) {
            trackedWindow.window->showNormal();
        } else {
            trackedWindow.window->showMaximized();
        }
    };
    const HWND hwnd = msg->hwnd;
    const auto finishMaximizeClick = [this, &trackedWindow, &toggleMaximized, hwnd](
                                         bool pointerInside) {
        trackedWindow.maximizeButtonTracking = false;
        if (GetCapture() == hwnd) {
            ReleaseCapture();
        }
        setMaximizeButtonPressed(false);
        setMaximizeButtonHovered(pointerInside);
        if (pointerInside) {
            toggleMaximized();
        }
    };

    switch (msg->message) {
    case WM_NCHITTEST: {
        POINT clientPoint{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        if (!ScreenToClient(msg->hwnd, &clientPoint)) {
            return false;
        }

        const LRESULT resizeResult = resizeHitTest(msg->hwnd, clientPoint);
        if (resizeResult != HTNOWHERE) {
            setMaximizeButtonHovered(false);
            setResult(resizeResult);
            return true;
        }

        if (itemContainsNativePoint(
                trackedWindow.maximizeButton,
                trackedWindow.window,
                clientPoint)) {
            setMaximizeButtonHovered(true);
            TRACKMOUSEEVENT tracking{
                sizeof(TRACKMOUSEEVENT),
                TME_LEAVE | TME_NONCLIENT,
                msg->hwnd,
                0,
            };
            TrackMouseEvent(&tracking);
            setResult(HTMAXBUTTON);
            return true;
        }

        setMaximizeButtonHovered(false);
        if (itemContainsNativePoint(
                trackedWindow.minimizeButton,
                trackedWindow.window,
                clientPoint)
            || itemContainsNativePoint(
                trackedWindow.closeButton,
                trackedWindow.window,
                clientPoint)) {
            setResult(HTCLIENT);
            return true;
        }
        if (itemContainsNativePoint(
                trackedWindow.systemMenu,
                trackedWindow.window,
                clientPoint)) {
            setResult(HTSYSMENU);
            return true;
        }
        if (itemContainsNativePoint(
                trackedWindow.titleBar,
                trackedWindow.window,
                clientPoint)) {
            setResult(HTCAPTION);
            return true;
        }
        return false;
    }
    case WM_NCMOUSEMOVE:
        setMaximizeButtonHovered(msg->wParam == HTMAXBUTTON);
        return false;
    case WM_NCMOUSELEAVE:
        setMaximizeButtonHovered(false);
        if (!trackedWindow.maximizeButtonTracking) {
            setMaximizeButtonPressed(false);
        }
        return false;
    case WM_NCLBUTTONDOWN:
        if (msg->wParam == HTMAXBUTTON) {
            trackedWindow.maximizeButtonTracking = true;
            SetCapture(msg->hwnd);
            setMaximizeButtonPressed(true);
            setResult(0);
            return true;
        }
        if (msg->wParam == HTCAPTION) {
            trackedWindow.window->startSystemMove();
            setResult(0);
            return true;
        }
        return false;
    case WM_NCLBUTTONDBLCLK:
        if (msg->wParam == HTCAPTION) {
            toggleMaximized();
            setResult(0);
            return true;
        }
        return false;
    case WM_MOUSEMOVE:
        if (trackedWindow.maximizeButtonTracking) {
            const POINT clientPoint{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            const bool pointerInside = itemContainsNativePoint(
                trackedWindow.maximizeButton,
                trackedWindow.window,
                clientPoint);
            setMaximizeButtonHovered(pointerInside);
            setMaximizeButtonPressed(pointerInside);
            setResult(0);
            return true;
        }
        return false;
    case WM_LBUTTONUP:
        if (trackedWindow.maximizeButtonTracking) {
            const POINT clientPoint{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            finishMaximizeClick(itemContainsNativePoint(
                trackedWindow.maximizeButton,
                trackedWindow.window,
                clientPoint));
            setResult(0);
            return true;
        }
        return false;
    case WM_NCLBUTTONUP:
        if (trackedWindow.maximizeButtonTracking) {
            POINT clientPoint{GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
            const bool pointAvailable = ScreenToClient(msg->hwnd, &clientPoint) != FALSE;
            finishMaximizeClick(
                pointAvailable
                && itemContainsNativePoint(
                    trackedWindow.maximizeButton,
                    trackedWindow.window,
                    clientPoint));
            setResult(0);
            return true;
        }
        setMaximizeButtonPressed(false);
        return false;
    case WM_CAPTURECHANGED:
        trackedWindow.maximizeButtonTracking = false;
        setMaximizeButtonPressed(false);
        return false;
    case WM_GETMINMAXINFO: {
        auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(msg->lParam);
        const HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{sizeof(MONITORINFO)};
        if (!GetMonitorInfoW(monitor, &monitorInfo)) {
            return false;
        }

        minMaxInfo->ptMaxPosition.x = monitorInfo.rcWork.left - monitorInfo.rcMonitor.left;
        minMaxInfo->ptMaxPosition.y = monitorInfo.rcWork.top - monitorInfo.rcMonitor.top;
        minMaxInfo->ptMaxSize.x = monitorInfo.rcWork.right - monitorInfo.rcWork.left;
        minMaxInfo->ptMaxSize.y = monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;
        const qreal devicePixelRatio = trackedWindow.window->devicePixelRatio();
        minMaxInfo->ptMinTrackSize.x = std::max(
            minMaxInfo->ptMinTrackSize.x,
            static_cast<LONG>(std::lround(
                trackedWindow.window->minimumWidth() * devicePixelRatio)));
        minMaxInfo->ptMinTrackSize.y = std::max(
            minMaxInfo->ptMinTrackSize.y,
            static_cast<LONG>(std::lround(
                trackedWindow.window->minimumHeight() * devicePixelRatio)));
        setResult(0);
        return true;
    }
    default:
        return false;
    }
}

void WindowChrome::setMaximizeButtonHovered(bool hovered)
{
    if (m_maximizeButtonHovered == hovered) {
        return;
    }
    m_maximizeButtonHovered = hovered;
    emit maximizeButtonHoveredChanged();
}

void WindowChrome::setMaximizeButtonPressed(bool pressed)
{
    if (m_maximizeButtonPressed == pressed) {
        return;
    }
    m_maximizeButtonPressed = pressed;
    emit maximizeButtonPressedChanged();
}
