#include "monitormanager.h"
#include "workerthreads.h"
#include "monitormanagerimpl.h"
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <algorithm>
#include "logmanager.h"
#include "usersettings.h"

MonitorWorker* MonitorManager::s_worker = nullptr;
QThread* MonitorManager::s_workerThread = nullptr;
MonitorManager* MonitorManager::s_instance = nullptr;
QList<Monitor> MonitorManager::s_cachedMonitors;
bool MonitorManager::s_nightLightEnabled = false;
bool MonitorManager::s_nightLightSupported = false;
QMutex MonitorManager::s_cacheMutex;

MonitorWorker::MonitorWorker(QObject *parent)
    : QObject(parent)
    , m_impl(nullptr)
    , m_ddcciBrightnessTimer(new QTimer(this))
    , m_pendingDDCCIBrightness(0)
    , m_hasPendingDDCCIBrightness(false)
{
    qRegisterMetaType<QList<Monitor>>();
    m_ddcciBrightnessTimer->setSingleShot(true);
    connect(m_ddcciBrightnessTimer, &QTimer::timeout, this, [this]() {
        if (m_hasPendingDDCCIBrightness) {
            int brightness = m_pendingDDCCIBrightness;
            m_hasPendingDDCCIBrightness = false;

            setDDCCIBrightness(brightness, m_ddcciDelayMs);
        }
    });

    if (UserSettings::instance()->allowBrightnessControl()) {
        m_pendingDDCCIBrightness = UserSettings::instance()->ddcciBrightness();
        m_ddcciDelayMs = UserSettings::instance()->ddcciQueueDelay();
        m_hasPendingDDCCIBrightness = true;
    }
}

void MonitorWorker::init()
{
    LOG_INFO("MonitorManager", "Initializing MonitorWorker on worker thread");

    m_impl = new MonitorManagerImpl();
    m_impl->setChangeCallback(
        [this]() { QMetaObject::invokeMethod(this, &MonitorWorker::enumerateMonitors, Qt::QueuedConnection); });

    enumerateMonitors();
    checkNightLightStatus();
    if (m_hasPendingDDCCIBrightness)
    {
        m_hasPendingDDCCIBrightness = false;
        setDDCCIBrightness(m_pendingDDCCIBrightness, m_ddcciDelayMs);
    }
}

MonitorWorker::~MonitorWorker()
{
    if (m_ddcciBrightnessTimer && m_ddcciBrightnessTimer->isActive()) {
        m_ddcciBrightnessTimer->stop();
    }

    if (m_impl) {
        delete m_impl;
        m_impl = nullptr;
    }
}

void MonitorWorker::cleanup()
{
    if (m_ddcciBrightnessTimer) {
        m_ddcciBrightnessTimer->stop();
        m_ddcciBrightnessTimer->deleteLater();
        m_ddcciBrightnessTimer = nullptr;
    }

    m_hasPendingDDCCIBrightness = false;

    if (m_impl) {
        delete m_impl;
        m_impl = nullptr;
    }
}

void MonitorWorker::enumerateMonitors()
{

    if (!m_impl) {
        return;
    }

    m_impl->enumerateMonitors();
    updateMonitorFromImpl();

    emit monitorsReady(m_monitors);
}

void MonitorWorker::updateMonitorFromImpl()
{
    m_monitors.clear();

    if (!m_impl) {
        return;
    }

    int count = m_impl->getMonitorCount();
    for (int i = 0; i < count; i++) {
        Monitor monitor;
        monitor.id = QString::fromStdWString(m_impl->getMonitorId(i));
        monitor.name = QString::fromStdWString(m_impl->getMonitorName(i));
        monitor.friendlyName = monitor.name;
        monitor.brightness = m_impl->getCachedBrightness(i);
        monitor.isSupported = m_impl->testDDCCI(i);
        monitor.isLaptopDisplay = m_impl->isLaptopDisplay(i);

        int currentBrightness = m_impl->getBrightnessInternal(i);
        if (currentBrightness != -1) {
            monitor.brightness = currentBrightness;
        } else {
            monitor.brightness = 50;
            monitor.isSupported = false;
        }

        monitor.minBrightness = 0;
        monitor.maxBrightness = 100;

        m_monitors.append(monitor);
    }
}

void MonitorWorker::setBrightness(const QString& monitorId, int brightness)
{

    if (!m_impl) {
        return;
    }

    int index = indexForId(monitorId);
    if (index < 0)
    {
        return;
    }

    if (m_impl->setBrightnessInternal(index, brightness)) {
        for (Monitor& monitor : m_monitors) {
            if (monitor.id == monitorId) {
                monitor.brightness = brightness;
                break;
            }
        }
        emit brightnessChanged(monitorId, brightness);
    }
}

void MonitorWorker::setBrightnessAll(int brightness)
{
    if (!m_impl)
        return;
    for (const auto& monitor : std::as_const(m_monitors))
    {
        if (monitor.isSupported)
            setBrightness(monitor.id, brightness);
    }
}

void MonitorWorker::refreshBrightnessLevels()
{

    if (!m_impl) {
        return;
    }


        for (Monitor& monitor : m_monitors) {
            if (monitor.isSupported) {
            int index = indexForId(monitor.id);
            if (index >= 0)
            {
                int brightness = m_impl->getBrightnessInternal(index);
                if (brightness != -1) {
                monitor.brightness = brightness;
            }
        }
    }
}

    emit monitorsReady(m_monitors);
}

void MonitorWorker::checkNightLightStatus()
{
    if (!m_impl) {
        emit nightLightStatusReady(false, false);
        return;
    }

    bool supported = m_impl->isNightLightSupported();
    bool enabled = supported ? m_impl->isNightLightEnabled() : false;

    emit nightLightStatusReady(supported, enabled);
}

void MonitorWorker::setNightLight(bool enabled)
{
    if (!m_impl) {
        return;
    }

    if (!m_impl->isNightLightSupported()) {
        return;
    }

    if (enabled) {
        m_impl->enableNightLight();
    } else {
        m_impl->disableNightLight();
    }

    // Check actual state after change
    bool actualEnabled = m_impl->isNightLightEnabled();
    emit nightLightChanged(actualEnabled);
}

void MonitorWorker::toggleNightLight()
{
    if (!m_impl) {
        return;
    }

    if (!m_impl->isNightLightSupported()) {
        return;
    }

    m_impl->toggleNightLight();

    // Check actual state after toggle
    bool enabled = m_impl->isNightLightEnabled();
    emit nightLightChanged(enabled);
}

void MonitorWorker::setDDCCIBrightness(int brightness, int delayMs)
{
    if (!m_impl || !m_ddcciBrightnessTimer)
        return;
    m_ddcciDelayMs = qBound(1, delayMs, 5000);
    brightness = qBound(0, brightness, 100);
    // If timer is already running, buffer this request
    if (m_ddcciBrightnessTimer->isActive()) {
        // If we already have a pending request, just update it (don't queue multiple)
        if (m_hasPendingDDCCIBrightness) {
            m_pendingDDCCIBrightness = brightness;
            return; // Just update the pending value and return
        }

        // Buffer this request
        m_pendingDDCCIBrightness = brightness;
        m_hasPendingDDCCIBrightness = true;
        return;
    }

    // Execute the brightness change immediately

    if (!m_impl) {
        return;
    }

    LOG_INFO("MonitorManager",
             QString("Setting DDC/CI brightness to %1% for all external monitors with delay %2ms").arg(brightness).arg(delayMs));

    bool anySuccess = false;

    // Set brightness for all DDC/CI-supported monitors (non-laptop displays)
    for (int i = 0; i < m_impl->getMonitorCount() && i < m_monitors.size(); i++)
    {
        if (!m_monitors[i].isLaptopDisplay && m_impl->testDDCCI(i)) {
            if (m_impl->setBrightnessInternal(i, brightness)) {
                // Update cached value in Qt monitor list
                if (i < m_monitors.size()) {
                    m_monitors[i].brightness = brightness;
                    emit brightnessChanged(m_monitors[i].id, brightness);
                }
                anySuccess = true;
                LOG_INFO("MonitorManager",
                         QString("Set DDC/CI brightness for monitor %1").arg(i));
            } else {
                LOG_WARN("MonitorManager",
                         QString("Failed to set DDC/CI brightness for monitor %1").arg(i));
            }
        }
    }

    if (anySuccess) {
        emit ddcciBrightnessChanged(brightness);
        LOG_INFO("MonitorManager",
                 "DDC/CI brightness change completed successfully");
    } else {
        LOG_WARN("MonitorManager",
                 "No DDC/CI monitors responded to brightness change");
    }

    // Start the timer to block subsequent calls
    m_ddcciBrightnessTimer->start(m_ddcciDelayMs);
}

void MonitorWorker::setWMIBrightness(int brightness)
{

    if (!m_impl) {
        return;
    }

    LOG_INFO("MonitorManager",
             QString("Setting WMI brightness to %1% for all laptop displays").arg(brightness));

    bool anySuccess = false;

    // Set brightness for all WMI-controlled monitors (laptop displays)
    for (int i = 0; i < m_impl->getMonitorCount() && i < m_monitors.size(); i++)
    {
        if (m_monitors[i].isLaptopDisplay) {
            if (m_impl->setBrightnessInternal(i, brightness)) {
                // Update cached value in Qt monitor list
                if (i < m_monitors.size()) {
                    m_monitors[i].brightness = brightness;
                    emit brightnessChanged(m_monitors[i].id, brightness);
                }
                anySuccess = true;
                LOG_INFO("MonitorManager",
                         QString("Set WMI brightness for monitor %1").arg(i));
            } else {
                LOG_WARN("MonitorManager",
                         QString("Failed to set WMI brightness for monitor %1").arg(i));
            }
        }
    }

    if (anySuccess) {
        emit wmiBrightnessChanged(brightness);
        LOG_INFO("MonitorManager",
                 "WMI brightness change completed successfully");
    } else {
        LOG_WARN("MonitorManager",
                 "No WMI monitors responded to brightness change");
    }
}

// MonitorManager implementation
MonitorManager::MonitorManager(QObject *parent)
    : QObject(parent)
    , m_monitorDetected(false)
    , m_currentBrightness(50)
    , m_nightLightEnabled(false)
    , m_nightLightSupported(false)
{
    s_instance = this;
    connect(UserSettings::instance(), &UserSettings::allowBrightnessControlChanged, this, [this] {
        if (UserSettings::instance()->allowBrightnessControl())
            initialize();
        else
            cleanup();
    });

    // Check if brightness control is enabled in settings
    bool allowBrightnessControl = UserSettings::instance()->allowBrightnessControl();

    if (allowBrightnessControl) {
        initialize();
    } else {
        LOG_INFO("MonitorManager", "Brightness control disabled in settings, skipping initialization");
    }
}

MonitorManager::~MonitorManager()
{
    cleanup();
    LOG_INFO("MonitorManager", "MonitorManager destructor called");
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void MonitorManager::initialize()
{
    if (s_worker)
        return;
    if (m_retiringThread) {
        m_initializeAfterRetirement = true;
        return;
    }
    m_initializeAfterRetirement = false;
    const auto generation = ++m_generation;
    s_workerThread = new QThread();
    s_worker = new MonitorWorker();
    s_worker->moveToThread(s_workerThread);

    auto acceptMonitors = [this, generation](const QList<Monitor>& monitors) {
        if (generation != m_generation)
            return;
                         updateCache(monitors);
        const auto first = std::find_if(monitors.cbegin(), monitors.cend(),
                                        [](const Monitor& monitor) { return monitor.isSupported; });
        const bool detected = first != monitors.cend();
        const int brightness = detected ? first->brightness : 50;
        if (m_monitorDetected != detected)
        {
            m_monitorDetected = detected;
            emit monitorDetectedChanged();
        }
        if (m_currentBrightness != brightness)
        {
            m_currentBrightness = brightness;
            emit brightnessChanged();
        }
        emit monitorsChanged(monitors);
    };
    connect(s_worker, &MonitorWorker::monitorsReady, this, acceptMonitors);
    connect(s_worker, &MonitorWorker::brightnessChanged, this,
            [this, generation, acceptMonitors](const QString& id, int brightness) {
                if (generation != m_generation)
                    return;
                auto monitors = getMonitors();
                for (auto& monitor : monitors)
                {
                    if (monitor.id == id)
                        monitor.brightness = brightness;
                }
                acceptMonitors(monitors);
                     });

    auto acceptNightLight = [this, generation](bool supported, bool enabled) {
        if (generation != m_generation)
            return;
        updateNightLightCache(supported, enabled);
        if (m_nightLightSupported != supported)
        {
            m_nightLightSupported = supported;
            emit nightLightSupportedChanged();
                             }
        if (m_nightLightEnabled != enabled)
        {
            m_nightLightEnabled = enabled;
            emit nightLightEnabledChanged();
                         }
    };
    connect(s_worker, &MonitorWorker::nightLightStatusReady, this, acceptNightLight);
    connect(s_worker, &MonitorWorker::nightLightChanged, this,
            [this, acceptNightLight](bool enabled) { acceptNightLight(m_nightLightSupported, enabled); });
    auto acceptBrightness = [this, generation](int brightness) {
        if (generation != m_generation)
            return;
        if (m_currentBrightness != brightness)
        {
            m_currentBrightness = brightness;
            emit brightnessChanged();
        }
    };
    connect(s_worker, &MonitorWorker::ddcciBrightnessChanged, this, acceptBrightness);
    connect(s_worker, &MonitorWorker::wmiBrightnessChanged, this, acceptBrightness);

    s_workerThread->start();
    QMetaObject::invokeMethod(s_worker, "init", Qt::QueuedConnection);
    LOG_INFO("MonitorManager", "MonitorManager initialization queued");
}

void MonitorManager::cleanup()
{
    ++m_generation;
    m_initializeAfterRetirement = false;
    LOG_INFO("MonitorManager", "MonitorManager cleanup requested");

    if (!s_workerThread) {
        LOG_INFO("MonitorManager", "MonitorManager already cleaned up, skipping");
        return;
    }

    auto* worker = s_worker;
    auto* thread = s_workerThread;
        s_worker = nullptr;
    s_workerThread = nullptr;
    disconnect(worker, nullptr, nullptr, nullptr);
    m_retiringThread = thread;
    connect(thread, &QThread::finished, this, [this, retired = QPointer<QThread>(thread)] {
        if (m_retiringThread != retired) return;
        m_retiringThread = nullptr;
        if (m_initializeAfterRetirement) initialize();
    });
    retireWorkerThread(thread, worker, "cleanup");

    // Clear cached data
    {
        QMutexLocker locker(&s_cacheMutex);
        s_cachedMonitors.clear();
        s_nightLightEnabled = false;
        s_nightLightSupported = false;
    }

    // Reset instance properties
    m_monitorDetected = false;
    m_currentBrightness = 50;
    m_nightLightEnabled = false;
    m_nightLightSupported = false;

    emit monitorDetectedChanged();
    emit brightnessChanged();
    emit nightLightEnabledChanged();
    emit nightLightSupportedChanged();

    LOG_INFO("MonitorManager", "MonitorManager cleanup complete");
}

MonitorWorker* MonitorManager::getWorker()
{
    return s_worker;
}

MonitorManager* MonitorManager::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!s_instance) {
        s_instance = new MonitorManager();
    }
    return s_instance;
}

MonitorManager* MonitorManager::instance()
{
    return s_instance;
}

// Async methods
void MonitorManager::enumerateMonitorsAsync()
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "enumerateMonitors", Qt::QueuedConnection);
    }
}

void MonitorManager::setBrightnessAsync(const QString& monitorId, int brightness)
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "setBrightness", Qt::QueuedConnection,
                                  Q_ARG(QString, monitorId), Q_ARG(int, brightness));
    }
}

void MonitorManager::setBrightnessAllAsync(int brightness)
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "setBrightnessAll", Qt::QueuedConnection,
                                  Q_ARG(int, brightness));
    }
}

void MonitorManager::refreshBrightnessLevelsAsync()
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "refreshBrightnessLevels", Qt::QueuedConnection);
    }
}

void MonitorManager::checkNightLightAsync()
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "checkNightLightStatus", Qt::QueuedConnection);
    }
}

void MonitorManager::setNightLightAsync(bool enabled)
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "setNightLight", Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }
}

void MonitorManager::toggleNightLightAsync()
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "toggleNightLight", Qt::QueuedConnection);
    }
}

// Cached getters
QList<Monitor> MonitorManager::getMonitors()
{
    QMutexLocker locker(&s_cacheMutex);
    return s_cachedMonitors;
}

int MonitorManager::getMonitorBrightness(const QString& monitorId)
{
    QMutexLocker locker(&s_cacheMutex);

    for (const Monitor& monitor : std::as_const(s_cachedMonitors)) {
        if (monitor.id == monitorId) {
            return monitor.brightness;
        }
    }
    return 50;
}

bool MonitorManager::getNightLightEnabled()
{
    QMutexLocker locker(&s_cacheMutex);
    return s_nightLightEnabled;
}

bool MonitorManager::getNightLightSupported()
{
    QMutexLocker locker(&s_cacheMutex);
    return s_nightLightSupported;
}

// QML Properties
bool MonitorManager::monitorDetected() const
{
    return m_monitorDetected;
}

int MonitorManager::brightness() const
{
    return m_currentBrightness;
}

bool MonitorManager::nightLightEnabled() const
{
    return m_nightLightEnabled;
}

bool MonitorManager::nightLightSupported() const
{
    return m_nightLightSupported;
}

// QML Methods
void MonitorManager::setBrightness(int value)
{
    setBrightnessAllAsync(value);
}

void MonitorManager::refreshMonitors()
{
    enumerateMonitorsAsync();
    checkNightLightAsync();
}

void MonitorManager::setNightLightEnabled(bool enabled)
{
    setNightLightAsync(enabled);
}

void MonitorManager::toggleNightLight()
{
    toggleNightLightAsync();
}

void MonitorManager::updateCache(const QList<Monitor>& monitors)
{
    QMutexLocker locker(&s_cacheMutex);
    s_cachedMonitors = monitors;
}

void MonitorManager::updateNightLightCache(bool supported, bool enabled)
{
    QMutexLocker locker(&s_cacheMutex);
    s_nightLightSupported = supported;
    s_nightLightEnabled = enabled;
}

void MonitorManager::setDDCCIBrightnessAsync(int brightness, int delayMs)
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "setDDCCIBrightness", Qt::QueuedConnection,
                                  Q_ARG(int, brightness), Q_ARG(int, delayMs));
    }
}

void MonitorManager::setWMIBrightnessAsync(int brightness)
{
    if (s_worker) {
        QMetaObject::invokeMethod(s_worker, "setWMIBrightness", Qt::QueuedConnection,
                                  Q_ARG(int, brightness));
    }
}

void MonitorManager::setDDCCIBrightness(int brightness, int delayMs)
{
    setDDCCIBrightnessAsync(brightness, delayMs);
}

void MonitorManager::setWMIBrightness(int brightness)
{
    setWMIBrightnessAsync(brightness);
}

int MonitorWorker::indexForId(const QString& id) const
{
    if (!m_impl)
        return -1;
    for (int index = 0; index < m_impl->getMonitorCount(); ++index)
    {
        if (QString::fromStdWString(m_impl->getMonitorId(index)) == id)
            return index;
    }
    return -1;
}
