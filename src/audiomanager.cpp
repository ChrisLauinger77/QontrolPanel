#include "audiomanager.h"
#include "workerthreads.h"
#include "headsetcontrolbridge.h"
#include "logmanager.h"
#include <QDebug>
#include <QMutexLocker>
#include <QMetaObject>
#include <QBuffer>
#include "nativeimage.h"
#include <QFileInfo>
#include <QPainter>
#include <QTimer>
#include <QSet>
#include <QRegularExpression>
#include <algorithm>
#include <atlbase.h>
#include <psapi.h>
#include <Shlobj.h>
#include <winver.h>
#include <Functiondiscoverykeys_devpkey.h>

AudioManager* AudioManager::m_instance = nullptr;
QMutex AudioManager::m_mutex;

// DeviceNotificationClient implementation
DeviceNotificationClient::DeviceNotificationClient(AudioWorker* worker)
    : m_cRef(1), m_callbackTarget(worker->callbackTarget())
{
}

DeviceNotificationClient::~DeviceNotificationClient()
{
}

HRESULT STDMETHODCALLTYPE DeviceNotificationClient::QueryInterface(REFIID riid, VOID **ppvInterface)
{
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IMMNotificationClient) == riid) {
        AddRef();
        *ppvInterface = (IMMNotificationClient*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG STDMETHODCALLTYPE DeviceNotificationClient::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE DeviceNotificationClient::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_cRef);
    if (0 == ulRef) {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE DeviceNotificationClient::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    //QString deviceId = QString::fromWCharArray(pwstrDeviceId);

    if (m_worker) {
        m_worker->requestDeviceRefresh();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceNotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    QString deviceId = QString::fromWCharArray(pwstrDeviceId);

    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "onDeviceAdded", Qt::QueuedConnection,
                                  Q_ARG(QString, deviceId));
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceNotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    QString deviceId = QString::fromWCharArray(pwstrDeviceId);

    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "onDeviceRemoved", Qt::QueuedConnection,
                                  Q_ARG(QString, deviceId));
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceNotificationClient::OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR pwstrDefaultDeviceId)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    QString deviceId = pwstrDefaultDeviceId ? QString::fromWCharArray(pwstrDefaultDeviceId) : QString();
    AudioWorker::DataFlow dataFlow = AudioWorker::fromWindowsDataFlow(flow);

    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "onDefaultDeviceChanged", Qt::QueuedConnection,
                                  Q_ARG(AudioWorker::DataFlow, dataFlow),
                                  Q_ARG(QString, deviceId));
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceNotificationClient::OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    if (m_worker) {
        m_worker->requestDeviceRefresh();
    }
    return S_OK;
}

// VolumeNotificationClient implementation
VolumeNotificationClient::VolumeNotificationClient(AudioWorker* worker, EDataFlow dataFlow)
    : m_cRef(1), m_callbackTarget(worker->callbackTarget()), m_dataFlow(dataFlow)
{
}

VolumeNotificationClient::~VolumeNotificationClient()
{
}

HRESULT STDMETHODCALLTYPE VolumeNotificationClient::QueryInterface(REFIID riid, VOID **ppvInterface)
{
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IAudioEndpointVolumeCallback) == riid) {
        AddRef();
        *ppvInterface = (IAudioEndpointVolumeCallback*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG STDMETHODCALLTYPE VolumeNotificationClient::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE VolumeNotificationClient::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_cRef);
    if (0 == ulRef) {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE VolumeNotificationClient::OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    if (m_worker && pNotify) {
        float volume = pNotify->fMasterVolume;
        bool muted = pNotify->bMuted;

        AudioWorker::DataFlow dataFlow = AudioWorker::fromWindowsDataFlow(m_dataFlow);

        QMetaObject::invokeMethod(m_worker, "onVolumeChanged", Qt::QueuedConnection,
                                  Q_ARG(AudioWorker::DataFlow, dataFlow),
                                  Q_ARG(float, volume),
                                  Q_ARG(bool, muted));
    }
    return S_OK;
}

// SessionNotificationClient implementation
SessionNotificationClient::SessionNotificationClient(AudioWorker* worker)
    : m_cRef(1), m_callbackTarget(worker->callbackTarget())
{
}

SessionNotificationClient::~SessionNotificationClient()
{
}

HRESULT STDMETHODCALLTYPE SessionNotificationClient::QueryInterface(REFIID riid, VOID **ppvInterface)
{
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IAudioSessionNotification) == riid) {
        AddRef();
        *ppvInterface = (IAudioSessionNotification*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG STDMETHODCALLTYPE SessionNotificationClient::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE SessionNotificationClient::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_cRef);
    if (0 == ulRef) {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE SessionNotificationClient::OnSessionCreated(IAudioSessionControl *NewSession)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    if (NewSession && m_callbackTarget->worker)
        m_callbackTarget->worker->requestApplicationRefresh();
    return S_OK;
}

// SessionEventsClient implementation
SessionEventsClient::SessionEventsClient(AudioWorker* worker, const QString& appId)
    : m_cRef(1), m_callbackTarget(worker->callbackTarget()), m_appId(appId)
{
}

SessionEventsClient::~SessionEventsClient()
{
}

HRESULT STDMETHODCALLTYPE SessionEventsClient::QueryInterface(REFIID riid, VOID **ppvInterface)
{
    if (IID_IUnknown == riid) {
        AddRef();
        *ppvInterface = (IUnknown*)this;
    } else if (__uuidof(IAudioSessionEvents) == riid) {
        AddRef();
        *ppvInterface = (IAudioSessionEvents*)this;
    } else {
        *ppvInterface = NULL;
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG STDMETHODCALLTYPE SessionEventsClient::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

ULONG STDMETHODCALLTYPE SessionEventsClient::Release()
{
    ULONG ulRef = InterlockedDecrement(&m_cRef);
    if (0 == ulRef) {
        delete this;
    }
    return ulRef;
}

HRESULT STDMETHODCALLTYPE SessionEventsClient::OnSimpleVolumeChanged(float NewVolume, BOOL NewMute, LPCGUID EventContext)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "onApplicationSessionVolumeChanged", Qt::QueuedConnection,
                                                 Q_ARG(QString, m_appId),
                                                 Q_ARG(float, NewVolume),
                                                 Q_ARG(bool, NewMute));
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SessionEventsClient::OnStateChanged(AudioSessionState NewState)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;

    if (NewState == AudioSessionStateExpired) {
        if (m_worker) {
            QMetaObject::invokeMethod(m_worker, "onSessionDisconnected", Qt::QueuedConnection);
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SessionEventsClient::OnSessionDisconnected(AudioSessionDisconnectReason DisconnectReason)
{
    QMutexLocker guard(&m_callbackTarget->mutex);
    auto* m_worker = m_callbackTarget->worker;
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "onSessionDisconnected", Qt::QueuedConnection);
    }
    return S_OK;
}

// Stub implementations for other required methods
HRESULT STDMETHODCALLTYPE SessionEventsClient::OnDisplayNameChanged(LPCWSTR NewDisplayName, LPCGUID EventContext) { return S_OK; }
HRESULT STDMETHODCALLTYPE SessionEventsClient::OnIconPathChanged(LPCWSTR NewIconPath, LPCGUID EventContext) { return S_OK; }
HRESULT STDMETHODCALLTYPE SessionEventsClient::OnChannelVolumeChanged(DWORD ChannelCount, float NewChannelVolumeArray[], DWORD ChangedChannel, LPCGUID EventContext) { return S_OK; }
HRESULT STDMETHODCALLTYPE SessionEventsClient::OnGroupingParamChanged(LPCGUID NewGroupingParam, LPCGUID EventContext) { return S_OK; }

// AudioWorker implementation
AudioWorker::AudioWorker()
    : m_deviceEnumerator(nullptr)
    , m_outputVolumeClient(nullptr)
    , m_inputVolumeClient(nullptr)
    , m_sessionNotificationClient(nullptr)
    , m_deviceNotificationClient(nullptr)
    , m_sessionManager(nullptr)
    , m_policyConfig(nullptr)
    , m_outputVolumeControl(nullptr)
    , m_inputVolumeControl(nullptr)
    , m_outputVolume(0)
    , m_inputVolume(0)
    , m_outputMuted(false)
    , m_inputMuted(false)
    , m_requestedSystemSoundsMute(std::nullopt)
    , m_audioLevelTimer(nullptr)
    , m_sessionManagerInvalid(false)
{
    qRegisterMetaType<AudioApplication>("AudioApplication");
    qRegisterMetaType<QList<AudioApplication>>("QList<AudioApplication>");
    qRegisterMetaType<AudioDevice>("AudioDevice");
    qRegisterMetaType<QList<AudioDevice>>("QList<AudioDevice>");
    qRegisterMetaType<HeadsetControlDevice>("HeadsetControlDevice");
    qRegisterMetaType<QList<HeadsetControlDevice>>("QList<HeadsetControlDevice>");

    m_callbackTarget->worker = this;
}

AudioWorker::~AudioWorker()
{
    cleanup();
}

void AudioWorker::initialize()
{
    LOG_INFO("AudioManager", "Initializing AudioWorker");

    HRESULT hr = initializeCOM();
    if (FAILED(hr)) {
        LOG_CRITICAL("AudioManager",
                                             QString("Failed to initialize COM, HRESULT: %1").arg(QString::number(hr, 16)));
        return;
    }

    LOG_INFO("AudioManager", "COM initialized successfully");

    // Create device enumerator
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&m_deviceEnumerator);
    if (FAILED(hr)) {
        LOG_CRITICAL("AudioManager",
                                             QString("Failed to create device enumerator, HRESULT: %1").arg(QString::number(hr, 16)));
        return;
    }

    LOG_INFO("AudioManager", "Device enumerator created successfully");

    // Create policy config for setting default devices
    hr = CoCreateInstance(__uuidof(CPolicyConfigClient), nullptr, CLSCTX_ALL,
                          __uuidof(IPolicyConfig), (void**)&m_policyConfig);
    if (FAILED(hr)) {
        LOG_WARN("AudioManager",
                 QString("Default-device switching unavailable, HRESULT: %1").arg(QString::number(hr, 16)));
            m_policyConfig = nullptr;
    } else {
        LOG_INFO("AudioManager", "Policy config created successfully");
    }

    // Set up notifications
    LOG_INFO("AudioManager", "Setting up audio notifications");
    setupVolumeNotifications();
    setupSessionNotifications();
    setupDeviceNotifications();

    // Get initial state
    LOG_INFO("AudioManager", "Getting initial audio state");
    updateCurrentVolumes();
    enumerateDevices();
    enumerateApplications();

    LOG_INFO("AudioManager", "AudioWorker initialization complete");
    emit initializationComplete();
}

void AudioWorker::cleanup()
{
    if (m_cleanedUp)
        return;
    m_cleanedUp = true;
    {
        QMutexLocker guard(&m_callbackTarget->mutex);
        m_callbackTarget->worker = nullptr;
    }

    if (m_audioLevelTimer) {
        m_audioLevelTimer->stop();
        delete m_audioLevelTimer;
        m_audioLevelTimer = nullptr;
    }

    for (auto it = m_sessionMeterControls.begin(); it != m_sessionMeterControls.end(); ++it) {
        if (it.value()) {
            it.value()->Release();
        }
    }
    m_sessionMeterControls.clear();
    m_applicationAudioLevels.clear();

    // Clean up volume controls cache
    for (auto it = m_sessionVolumeControls.begin(); it != m_sessionVolumeControls.end(); ++it) {
        if (it.value()) {
            it.value()->Release();
        }
    }
    m_sessionVolumeControls.clear();

    // Clean up session notifications SECOND
    if (m_sessionManager && m_sessionNotificationClient) {
        m_sessionManager->UnregisterSessionNotification(m_sessionNotificationClient);
        m_sessionNotificationClient->Release();
        m_sessionNotificationClient = nullptr;
    }
    if (m_sessionManager)
    {
        m_sessionManager->Release();
        m_sessionManager = nullptr;
    }

    // Clean up device notifications
    if (m_deviceEnumerator && m_deviceNotificationClient) {
        m_deviceEnumerator->UnregisterEndpointNotificationCallback(m_deviceNotificationClient);
    }
    if (m_deviceNotificationClient) {
        m_deviceNotificationClient->Release();
        m_deviceNotificationClient = nullptr;
    }

    // Clean up active sessions
    for (auto& sessionInfo : m_activeSessions) {
        if (sessionInfo.sessionControl && sessionInfo.eventsClient) {
            sessionInfo.sessionControl->UnregisterAudioSessionNotification(sessionInfo.eventsClient);
        }
        if (sessionInfo.eventsClient) {
            sessionInfo.eventsClient->Release();
        }
        if (sessionInfo.sessionControl) {
            sessionInfo.sessionControl->Release();
        }
    }
    m_activeSessions.clear();

    // Cleanup volume notifications
    if (m_outputVolumeControl)
    {
        if (m_outputVolumeClient)
        m_outputVolumeControl->UnregisterControlChangeNotify(m_outputVolumeClient);
        m_outputVolumeControl->Release();
        m_outputVolumeControl = nullptr;
    }

    if (m_inputVolumeControl)
    {
        if (m_inputVolumeClient)
        m_inputVolumeControl->UnregisterControlChangeNotify(m_inputVolumeClient);
        m_inputVolumeControl->Release();
        m_inputVolumeControl = nullptr;
    }

    if (m_outputVolumeClient) {
        m_outputVolumeClient->Release();
        m_outputVolumeClient = nullptr;
    }

    if (m_inputVolumeClient) {
        m_inputVolumeClient->Release();
        m_inputVolumeClient = nullptr;
    }

    if (m_policyConfig) {
        m_policyConfig->Release();
        m_policyConfig = nullptr;
    }

    if (m_deviceEnumerator) {
        m_deviceEnumerator->Release();
        m_deviceEnumerator = nullptr;
    }

    m_cachedHeadsetDevices.clear();
    if (m_comInitialized)
    {
        CoUninitialize();
        m_comInitialized = false;
    }
}

void AudioWorker::onHeadsetDataUpdated(const QList<HeadsetControlDevice>& headsetDevices)
{
    m_cachedHeadsetDevices = headsetDevices;
    updateDevicesBatteryInfo(m_cachedHeadsetDevices);
    emit devicesChanged(m_devices);
}

void AudioWorker::updateDevicesBatteryInfo(const QList<HeadsetControlDevice>& headsetDevices)
{

    for (AudioDevice& audioDevice : m_devices) {
        bool foundMatch = false;
        bool hadHeadsetBattery = (audioDevice.batteryPercentage != -1);

        // Find matching headset by VID/PID (only if we have devices to check)
        for (const HeadsetControlDevice& headsetDevice : headsetDevices) {
            QString deviceVid = headsetDevice.vendorId.startsWith("0x", Qt::CaseInsensitive) ? headsetDevice.vendorId.mid(2) : headsetDevice.vendorId;
            QString devicePid = headsetDevice.productId.startsWith("0x", Qt::CaseInsensitive) ? headsetDevice.productId.mid(2) : headsetDevice.productId;
            QString audioVid = audioDevice.vendorId.startsWith("0x", Qt::CaseInsensitive) ? audioDevice.vendorId.mid(2) : audioDevice.vendorId;
            QString audioPid = audioDevice.productId.startsWith("0x", Qt::CaseInsensitive) ? audioDevice.productId.mid(2) : audioDevice.productId;

            if (deviceVid.compare(audioVid, Qt::CaseInsensitive) == 0 &&
                devicePid.compare(audioPid, Qt::CaseInsensitive) == 0) {
                audioDevice.batteryPercentage = headsetDevice.batteryLevel;
                audioDevice.batteryStatus = headsetDevice.batteryStatus;
                foundMatch = true;
                break;
            }
        }

        if (hadHeadsetBattery && !foundMatch) {
            audioDevice.batteryPercentage = -1;
            audioDevice.batteryStatus = "BATTERY_UNAVAILABLE";
        }
    }
}

void AudioWorker::initializeAudioLevelTimer()
{
    if (!m_audioLevelTimer) {
        m_audioLevelTimer = new QTimer(this);
        connect(m_audioLevelTimer, &QTimer::timeout, this, &AudioWorker::updateAudioLevels);
    }
}

void AudioWorker::startAudioLevelMonitoring()
{
    if (!m_audioLevelTimer) {
        initializeAudioLevelTimer();
    }
    m_audioLevelTimer->start(100); // Update every 100ms
}

void AudioWorker::stopAudioLevelMonitoring()
{
    if (m_audioLevelTimer) {
        m_audioLevelTimer->stop();
    }
}

void AudioWorker::updateAudioLevels()
{
    // Get device audio levels
    int outputLevel = getDeviceAudioLevel(eRender);
    int inputLevel = getDeviceAudioLevel(eCapture);

    emit outputAudioLevelChanged(outputLevel);
    emit inputAudioLevelChanged(inputLevel);

    // Add application levels
    updateApplicationAudioLevels();
}

// Add these helper methods to AudioWorker
int AudioWorker::getDeviceAudioLevel(EDataFlow dataFlow)
{
    if (!m_deviceEnumerator) return 0;

    CComPtr<IMMDevice> device;
    HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, &device);
    if (FAILED(hr)) return 0;

    CComPtr<IAudioMeterInformation> meterInfo;
    hr = device->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, (void**)&meterInfo);
    if (FAILED(hr)) return 0;

    float peakLevel = 0.0f;
    hr = meterInfo->GetPeakValue(&peakLevel);
    if (FAILED(hr)) return 0;

    return static_cast<int>(peakLevel * 100);
}

HRESULT AudioWorker::initializeCOM()
{
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_comInitialized = SUCCEEDED(hr);
    return hr;
}

void AudioWorker::setupVolumeNotifications()
{
    // Clean up existing volume notifications first
    if (m_outputVolumeControl && m_outputVolumeClient) {
        m_outputVolumeControl->UnregisterControlChangeNotify(m_outputVolumeClient);
        m_outputVolumeControl->Release();
        m_outputVolumeControl = nullptr;
        m_outputVolumeClient->Release();
        m_outputVolumeClient = nullptr;
    }

    if (m_inputVolumeControl && m_inputVolumeClient) {
        m_inputVolumeControl->UnregisterControlChangeNotify(m_inputVolumeClient);
        m_inputVolumeControl->Release();
        m_inputVolumeControl = nullptr;
        m_inputVolumeClient->Release();
        m_inputVolumeClient = nullptr;
    }

    if (!m_deviceEnumerator) return;

    // Setup output volume notifications
    CComPtr<IMMDevice> outputDevice;
    HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &outputDevice);
    if (SUCCEEDED(hr)) {
        hr = outputDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                    nullptr, (void**)&m_outputVolumeControl);
        if (SUCCEEDED(hr)) {
            m_outputVolumeClient = new VolumeNotificationClient(this, eRender);
            m_outputVolumeControl->RegisterControlChangeNotify(m_outputVolumeClient);
        }
    }

    // Setup input volume notifications
    CComPtr<IMMDevice> inputDevice;
    hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &inputDevice);
    if (SUCCEEDED(hr)) {
        hr = inputDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                   nullptr, (void**)&m_inputVolumeControl);
        if (SUCCEEDED(hr)) {
            m_inputVolumeClient = new VolumeNotificationClient(this, eCapture);
            m_inputVolumeControl->RegisterControlChangeNotify(m_inputVolumeClient);
        }
    }

    // Update current volumes for the new devices
    updateCurrentVolumes();
}

void AudioWorker::setupSessionNotifications()
{
    LOG_INFO("AudioManager", "Setting up session notifications");

    if (m_sessionManager && m_sessionNotificationClient)
        m_sessionManager->UnregisterSessionNotification(m_sessionNotificationClient);
    if (m_sessionNotificationClient)
    {
        m_sessionNotificationClient->Release();
        m_sessionNotificationClient = nullptr;
    }
    if (m_sessionManager)
    {
        m_sessionManager->Release();
        m_sessionManager = nullptr;
    }

    if (!m_deviceEnumerator) {
        LOG_CRITICAL("AudioManager", "No device enumerator available for session notifications");
        return;
    }

    CComPtr<IMMDevice> device;
    HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        LOG_CRITICAL("AudioManager",
                                             QString("Failed to get default audio endpoint for session notifications, HRESULT: %1").arg(QString::number(hr, 16)));
        return;
    }

    LOG_INFO("AudioManager", "Got default audio endpoint for session notifications");

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                          nullptr, (void**)&m_sessionManager);
    if (FAILED(hr)) {
        LOG_CRITICAL("AudioManager",
                                             QString("Failed to activate session manager, HRESULT: %1").arg(QString::number(hr, 16)));
        return;
    }

    LOG_INFO("AudioManager", "Session manager activated successfully");

    m_sessionNotificationClient = new SessionNotificationClient(this);
    hr = m_sessionManager->RegisterSessionNotification(m_sessionNotificationClient);
    if (SUCCEEDED(hr)) {
        m_sessionManagerInvalid = false; // Clear the invalid flag on successful setup
        LOG_INFO("AudioManager", "Session notifications registered successfully");
    } else {
        LOG_CRITICAL("AudioManager",
                                             QString("Failed to register session notifications, HRESULT: %1").arg(QString::number(hr, 16)));
        m_sessionNotificationClient->Release();
        m_sessionNotificationClient = nullptr;
    }
}

int AudioWorker::getApplicationAudioLevel(const QString& appId)
{
    if (appId == "system_sounds") {
        return 0;
    }

    auto it = m_sessionMeterControls.find(appId);
    if (it == m_sessionMeterControls.end()) {
        return 0;
    }

    float peakLevel = 0.0f;
    HRESULT hr = it.value()->GetPeakValue(&peakLevel);
    if (FAILED(hr)) {
        return 0;
    }

    return static_cast<int>(peakLevel * 100);
}

void AudioWorker::updateApplicationAudioLevels()
{
    for (auto it = m_sessionMeterControls.begin(); it != m_sessionMeterControls.end(); ++it) {
        int newLevel = getApplicationAudioLevel(it.key());
        int cachedLevel = m_applicationAudioLevels.value(it.key(), -1);

        if (newLevel != cachedLevel) {
            m_applicationAudioLevels[it.key()] = newLevel;
            emit applicationAudioLevelChanged(it.key(), newLevel);
        }
    }
}

void AudioWorker::startApplicationAudioLevelMonitoring()
{
    if (!m_audioLevelTimer) {
        initializeAudioLevelTimer();
    }
    m_audioLevelTimer->start(100);
}

void AudioWorker::stopApplicationAudioLevelMonitoring()
{
    if (m_audioLevelTimer) {
        m_audioLevelTimer->stop();
    }
}

void AudioWorker::setupDeviceNotifications()
{
    if (!m_deviceEnumerator) return;

    m_deviceNotificationClient = new DeviceNotificationClient(this);
    HRESULT hr = m_deviceEnumerator->RegisterEndpointNotificationCallback(m_deviceNotificationClient);
    if (SUCCEEDED(hr)) {
        // Success - no additional logging needed since the original was empty
    } else {
        LOG_CRITICAL("AudioManager",
                                             QString("Failed to register device notifications, HRESULT: %1").arg(QString::number(hr, 16)));
        m_deviceNotificationClient->Release();
        m_deviceNotificationClient = nullptr;
    }
}

void AudioWorker::updateCurrentVolumes()
{
    // Get current output volume and mute state
    if (m_outputVolumeControl) {
        float volume = 0.0f;
        BOOL muted = FALSE;

        if (SUCCEEDED(m_outputVolumeControl->GetMasterVolumeLevelScalar(&volume))) {
            m_outputVolume = static_cast<int>(std::round(volume * 100.0f));
            emit outputVolumeChanged(m_outputVolume);
        }

        if (SUCCEEDED(m_outputVolumeControl->GetMute(&muted))) {
            m_outputMuted = muted;
            emit outputMuteChanged(m_outputMuted);
        }
    }

    // Get current input volume and mute state
    if (m_inputVolumeControl) {
        float volume = 0.0f;
        BOOL muted = FALSE;

        if (SUCCEEDED(m_inputVolumeControl->GetMasterVolumeLevelScalar(&volume))) {
            m_inputVolume = static_cast<int>(std::round(volume * 100.0f));
            emit inputVolumeChanged(m_inputVolume);
        }

        if (SUCCEEDED(m_inputVolumeControl->GetMute(&muted))) {
            m_inputMuted = muted;
            emit inputMuteChanged(m_inputMuted);
        }
    }
}

void AudioWorker::enumerateDevices()
{
    if (!m_deviceEnumerator) {
        LOG_CRITICAL("AudioManager", "No device enumerator available");
        return;
    }

    QList<AudioDevice> newDevices;

    // Enumerate both input and output devices
    for (int dataFlowIndex = 0; dataFlowIndex < 2; ++dataFlowIndex) {
        EDataFlow dataFlow = (dataFlowIndex == 0) ? eRender : eCapture;
        QString flowName = (dataFlow == eRender) ? "Output" : "Input";

        CComPtr<IMMDeviceCollection> deviceCollection;
        HRESULT hr = m_deviceEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &deviceCollection);
        if (FAILED(hr)) {
            LOG_CRITICAL("AudioManager",
                                                 QString("Failed to enumerate %1 devices, HRESULT: %2").arg(flowName, QString::number(hr, 16)));
            continue;
        }

        UINT deviceCount = 0;
        deviceCollection->GetCount(&deviceCount);

        CComPtr<IMMDevice> defaultDevice;
        CComPtr<IMMDevice> defaultCommDevice;
        m_deviceEnumerator->GetDefaultAudioEndpoint(dataFlow, eConsole, &defaultDevice);
        m_deviceEnumerator->GetDefaultAudioEndpoint(dataFlow, eCommunications, &defaultCommDevice);

        LPWSTR defaultDeviceId = nullptr;
        LPWSTR defaultCommDeviceId = nullptr;
        if (defaultDevice) {
            defaultDevice->GetId(&defaultDeviceId);
        }
        if (defaultCommDevice) {
            defaultCommDevice->GetId(&defaultCommDeviceId);
        }

        for (UINT i = 0; i < deviceCount; ++i) {
            CComPtr<IMMDevice> device;
            hr = deviceCollection->Item(i, &device);
            if (FAILED(hr)) continue;

            AudioDevice audioDevice = createAudioDeviceFromInterface(device, dataFlow);
            if (!audioDevice.id.isEmpty()) {
                if (defaultDeviceId && audioDevice.id == QString::fromWCharArray(defaultDeviceId)) {
                    audioDevice.isDefault = true;
                }
                if (defaultCommDeviceId && audioDevice.id == QString::fromWCharArray(defaultCommDeviceId)) {
                    audioDevice.isDefaultCommunication = true;
                }

                newDevices.append(audioDevice);
            }
        }

        if (defaultDeviceId) {
            CoTaskMemFree(defaultDeviceId);
        }
        if (defaultCommDeviceId) {
            CoTaskMemFree(defaultCommDeviceId);
        }
    }

    m_devices = newDevices;


    updateDevicesBatteryInfo(m_cachedHeadsetDevices);

    emit devicesChanged(m_devices);
}

AudioDevice AudioWorker::createAudioDeviceFromInterface(IMMDevice* device, EDataFlow dataFlow)
{
    AudioDevice audioDevice;
    if (!device) return audioDevice;

    // Get device ID
    LPWSTR deviceId = nullptr;
    HRESULT hr = device->GetId(&deviceId);
    if (SUCCEEDED(hr) && deviceId) {
        audioDevice.id = QString::fromWCharArray(deviceId);
        CoTaskMemFree(deviceId);
    }

    // Get device state
    DWORD state = 0;
    hr = device->GetState(&state);
    if (SUCCEEDED(hr)) {
        switch (state) {
        case DEVICE_STATE_ACTIVE:
            audioDevice.state = "Active";
            break;
        case DEVICE_STATE_DISABLED:
            audioDevice.state = "Disabled";
            break;
        case DEVICE_STATE_NOTPRESENT:
            audioDevice.state = "Not Present";
            break;
        case DEVICE_STATE_UNPLUGGED:
            audioDevice.state = "Unplugged";
            break;
        default:
            audioDevice.state = "Unknown";
            break;
        }
    }

    // Get device properties
    audioDevice.name = getDeviceProperty(device, PKEY_Device_FriendlyName);
    audioDevice.description = getDeviceProperty(device, PKEY_Device_DeviceDesc);
    audioDevice.isInput = (dataFlow == eCapture);

    if (audioDevice.name.isEmpty()) {
        audioDevice.name = audioDevice.description;
    }
    if (audioDevice.name.isEmpty()) {
        audioDevice.name = "Unknown Device";
    }

    // Extract USB VID/PID only
    CComPtr<IPropertyStore> propertyStore;
    hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
    if (SUCCEEDED(hr)) {
        DWORD propertyCount = 0;
        hr = propertyStore->GetCount(&propertyCount);
        if (SUCCEEDED(hr)) {
            for (DWORD i = 0; i < propertyCount; ++i) {
                PROPERTYKEY propertyKey;
                PROPVARIANT propVariant;
                PropVariantInit(&propVariant);

                hr = propertyStore->GetAt(i, &propertyKey);
                if (SUCCEEDED(hr)) {
                    hr = propertyStore->GetValue(propertyKey, &propVariant);
                    if (SUCCEEDED(hr) && propVariant.vt == VT_LPWSTR) {
                        QString propValue = QString::fromWCharArray(propVariant.pwszVal);

                        // Look for USB VID/PID pattern
                        if (audioDevice.vendorId.isEmpty() && propValue.contains("VID_") && propValue.contains("PID_")) {
                            QRegularExpression regex(R"(VID_([0-9A-F]{4}).*PID_([0-9A-F]{4}))");
                            QRegularExpressionMatch match = regex.match(propValue);
                            if (match.hasMatch()) {
                                audioDevice.vendorId = match.captured(1);
                                audioDevice.productId = match.captured(2);
                            }
                        }
                    }
                }
                PropVariantClear(&propVariant);
            }
        }
    }

    return audioDevice;
}

QString AudioWorker::getDeviceProperty(IMMDevice* device, const PROPERTYKEY& key)
{
    if (!device) return QString();

    CComPtr<IPropertyStore> propertyStore;
    HRESULT hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
    if (FAILED(hr)) return QString();

    PROPVARIANT propVariant;
    PropVariantInit(&propVariant);

    hr = propertyStore->GetValue(key, &propVariant);
    if (SUCCEEDED(hr) && propVariant.vt == VT_LPWSTR) {
        QString result = QString::fromWCharArray(propVariant.pwszVal);
        PropVariantClear(&propVariant);
        return result;
    }

    PropVariantClear(&propVariant);
    return QString();
}

void AudioWorker::enumerateApplications()
{
    if (m_cleanedUp)
        return;
    if (!ensureValidSessionManager()) {
        LOG_CRITICAL("AudioManager", "Failed to restore session manager, skipping application enumeration");
        return;
    }

    QList<AudioApplication> newApplications;
    bool foundSystemSounds = false;

    IAudioSessionManager2* sessionManager = m_sessionManager;
    CComPtr<IAudioSessionEnumerator> pSessionEnumerator;
    HRESULT hr = sessionManager->GetSessionEnumerator(&pSessionEnumerator);
    if (FAILED(hr))
    {
        LOG_CRITICAL("AudioManager", "Failed to get session enumerator");
        return;
    }

    int sessionCount = 0;
    hr = pSessionEnumerator->GetCount(&sessionCount);
    if (FAILED(hr))
    {
        LOG_WARN("AudioManager", "Could not obtain session count; retaining previous snapshot");
        return;
    }
    // Clean up existing volume controls cache
    for (auto it = m_sessionVolumeControls.begin(); it != m_sessionVolumeControls.end(); ++it) {
        if (it.value()) {
            it.value()->Release();
        }
    }
    m_sessionVolumeControls.clear();

    // Clean up existing meter controls
    for (auto it = m_sessionMeterControls.begin(); it != m_sessionMeterControls.end(); ++it) {
        if (it.value()) {
            it.value()->Release();
        }
    }
    m_sessionMeterControls.clear();

    // Clean up existing sessions
    for (auto& sessionInfo : m_activeSessions) {
        if (sessionInfo.sessionControl && sessionInfo.eventsClient) {
            sessionInfo.sessionControl->UnregisterAudioSessionNotification(sessionInfo.eventsClient);
        }
        if (sessionInfo.eventsClient) {
            sessionInfo.eventsClient->Release();
        }
        if (sessionInfo.sessionControl) {
            sessionInfo.sessionControl->Release();
        }
    }
    m_activeSessions.clear();



    // First pass: collect all session data
    struct TempSessionData {
        IAudioSessionControl* sessionControl;
        ISimpleAudioVolume* volumeControl;
        IAudioMeterInformation* meterControl;
        QString appId;
        QString executableName;
        QString finalAppName;
        QString iconPath;
        int volume;
        bool isMuted;
        bool isSystemSounds;
        DWORD processId;
        int sessionIndex;
        QString sessionDisplayName;
        QString sessionIdentifier;
        SIZE_T memorySnapshot = 0;
    };

    QList<TempSessionData> tempSessions;
    QMap<DWORD, SIZE_T> processMemorySnapshots;

    for (int i = 0; i < sessionCount; ++i) {
        IAudioSessionControl* sessionControl = nullptr;
        hr = pSessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr) || !sessionControl) {
            continue;
        }

        CComPtr<IAudioSessionControl2> sessionControl2;
        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        if (FAILED(hr)) {
            sessionControl->Release();
            continue;
        }

        DWORD processId = 0;
        hr = sessionControl2->GetProcessId(&processId);
        if (FAILED(hr)) {
            sessionControl->Release();
            continue;
        }

        // Get session display name first to check for system sounds
        LPWSTR pwszDisplayName = nullptr;
        hr = sessionControl->GetDisplayName(&pwszDisplayName);
        QString sessionDisplayName = SUCCEEDED(hr) ? QString::fromWCharArray(pwszDisplayName) : "Unknown Application";
        CoTaskMemFree(pwszDisplayName);

        AudioSessionState state;
        if (FAILED(sessionControl->GetState(&state)) || state == AudioSessionStateExpired)
        {
            sessionControl->Release();
            continue;
        }
        // Inactive sessions remain controllable; expired sessions do not.
        bool isSystemSounds = isSystemSoundsSession(sessionControl2);

        if (!isSystemSounds && processId == GetCurrentProcessId()) {
            sessionControl->Release();
            continue;
        }

        if (isSystemSounds) {
            foundSystemSounds = true;
        }

        // Get volume info and cache the volume control
        CComPtr<ISimpleAudioVolume> pSimpleAudioVolume;
        hr = sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pSimpleAudioVolume);
        if (FAILED(hr)) {
            sessionControl->Release();
            continue;
        }

        // Get meter info for audio levels - but exclude system sounds
        CComPtr<IAudioMeterInformation> pMeterInfo;
        if (!isSystemSounds) {
            hr = sessionControl->QueryInterface(__uuidof(IAudioMeterInformation), (void**)&pMeterInfo);
            if (FAILED(hr)) {
                LOG_WARN("AudioManager", QString("Failed to get meter info for session %1").arg(i));
            }
        }

        if (isSystemSounds && m_requestedSystemSoundsMute.has_value()) {
            HRESULT setMuteHr = pSimpleAudioVolume->SetMute(*m_requestedSystemSoundsMute, nullptr);
            if (FAILED(setMuteHr)) {
                LOG_WARN("AudioManager",
                         QString("Failed to apply requested system sounds mute during enumeration, HRESULT: %1")
                             .arg(QString::number(setMuteHr, 16)));
            }
        }

        BOOL isMuted = FALSE;
        pSimpleAudioVolume->GetMute(&isMuted);

        float volumeLevel = 0.0f;
        pSimpleAudioVolume->GetMasterVolume(&volumeLevel);

        // Process application info
        TempSessionData tempData;
        tempData.sessionControl = sessionControl;
        tempData.volumeControl = pSimpleAudioVolume.Detach(); // Detach from CComPtr
        tempData.meterControl = isSystemSounds ? nullptr : pMeterInfo.Detach(); // Only assign meter for non-system sounds
        tempData.volume = static_cast<int>(volumeLevel * 100);
        tempData.isMuted = isMuted;
        tempData.isSystemSounds = isSystemSounds;
        tempData.processId = processId;
        if (!processMemorySnapshots.contains(processId))
        {
            SIZE_T bytes = 0;
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (process)
            {
                PROCESS_MEMORY_COUNTERS counters{};
                if (GetProcessMemoryInfo(process, &counters, sizeof(counters)))
                    bytes = counters.WorkingSetSize;
                CloseHandle(process);
            }
            processMemorySnapshots[processId] = bytes;
        }
        tempData.memorySnapshot = processMemorySnapshots.value(processId);
        tempData.sessionIndex = i;
        tempData.sessionDisplayName = sessionDisplayName;
        LPWSTR identifier = nullptr;
        if (SUCCEEDED(sessionControl2->GetSessionInstanceIdentifier(&identifier)))
        {
            tempData.sessionIdentifier = QString::fromWCharArray(identifier);
        }
        CoTaskMemFree(identifier);

        QString executablePath;
        QString executableName;
        QString finalAppName = sessionDisplayName;

        // Handle special case for Windows system sounds first
        if (isSystemSounds) {
            finalAppName = tr("System sounds");
            executableName = tr("System sounds");
            tempData.appId = "system_sounds";
            tempData.iconPath = "";
        }
        else {
            // Handle regular applications
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
            if (hProcess) {
                WCHAR exePath[MAX_PATH];
                if (GetModuleFileNameEx(hProcess, NULL, exePath, MAX_PATH)) {
                    executablePath = QString::fromWCharArray(exePath);
                    QFileInfo fileInfo(executablePath);
                    executableName = fileInfo.baseName();

                    if (!executableName.isEmpty()) {
                        executableName[0] = executableName[0].toUpper();
                    }

                    // Try to get better display name from version info
                    QString betterDisplayName = getApplicationDisplayName(executablePath);
                    if (!betterDisplayName.isEmpty()) {
                        finalAppName = betterDisplayName;
                    } else if (!executableName.isEmpty()) {
                        finalAppName = executableName;
                    }

                    // Get icon using the improved method
                    QImage appIcon = getApplicationIcon(executablePath);
                    if (!appIcon.isNull()) {
                        QImage pixmap = appIcon.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        QByteArray byteArray;
                        QBuffer buffer(&byteArray);
                        buffer.open(QIODevice::WriteOnly);
                        pixmap.save(&buffer, "PNG");
                        tempData.iconPath = "data:image/png;base64," + byteArray.toBase64();
                    }
                }
                CloseHandle(hProcess);
            }

            tempData.appId = tempData.sessionIdentifier;
        }

        tempData.executableName = executableName;
        tempData.finalAppName = finalAppName;

        if (finalAppName.trimmed().isEmpty() || tempData.appId.isEmpty())
        {
            if (tempData.volumeControl)
                tempData.volumeControl->Release();
            if (tempData.meterControl)
                tempData.meterControl->Release();
            sessionControl->Release();
            continue;
        }

        tempSessions.append(tempData);
    }

    QMap<QString, QList<TempSessionData*>> executableGroups;

    for (int i = 0; i < tempSessions.count(); ++i) {
        TempSessionData& tempData = tempSessions[i];
        if (!executableGroups.contains(tempData.executableName)) {
            executableGroups[tempData.executableName] = QList<TempSessionData*>();
        }
        executableGroups[tempData.executableName].append(&tempData);
    }

    for (auto& group : executableGroups) {
        std::sort(group.begin(), group.end(), [](const TempSessionData* a, const TempSessionData* b) {
            if (a->memorySnapshot != b->memorySnapshot)
                return a->memorySnapshot > b->memorySnapshot;
            return a->sessionIdentifier < b->sessionIdentifier;
                  });
    }

    // Keep legacy initial ordering, but never renumber a surviving stream when
    // memory use or sibling sessions change. Native queries stay outside sorting.
    QList<QPair<QString, QString>> orderedSessions;
    for (auto group = executableGroups.cbegin(); group != executableGroups.cend(); ++group)
    {
        for (const auto* session : group.value())
            orderedSessions.append(qMakePair(group.key(), session->appId));
    }
    m_streamIndices.update(orderedSessions);

    // Third pass: create final applications with stable stream indices
    for (const auto& group : executableGroups) {
        for (int streamIndex = 0; streamIndex < group.count(); ++streamIndex) {
            const TempSessionData* tempData = group[streamIndex];

            AudioApplication app;
            app.id = tempData->appId;
            app.name = tempData->finalAppName;
            app.executableName = tempData->executableName;
            app.iconPath = tempData->iconPath;
            app.volume = tempData->volume;
            app.isMuted = tempData->isMuted;
            app.streamIndex = tempData->isSystemSounds ? 0 : m_streamIndices.index(app.executableName, app.id);
            app.isSystemSounds = tempData->isSystemSounds;

            // Cache the volume control
            m_sessionVolumeControls[app.id] = tempData->volumeControl;

            // Cache the meter control for audio levels - but only for non-system sounds
            if (tempData->meterControl && !tempData->isSystemSounds) {
                m_sessionMeterControls[app.id] = tempData->meterControl;
            }

            // Register session events for real-time updates
            SessionEventsClient* eventsClient = new SessionEventsClient(this, app.id);
            hr = tempData->sessionControl->RegisterAudioSessionNotification(eventsClient);
            if (SUCCEEDED(hr)) {
                SessionInfo sessionInfo;
                sessionInfo.sessionControl = tempData->sessionControl; // Keep alive
                sessionInfo.eventsClient = eventsClient;
                sessionInfo.appId = app.id;
                m_activeSessions.append(sessionInfo);
            } else {
                eventsClient->Release();
                tempData->sessionControl->Release();
                LOG_WARN("AudioManager", QString("Failed to register events for app: %1").arg(app.name));
            }

            newApplications.append(app);
        }
    }

    // If we didn't find system sounds in the sessions, create a default entry
    if (!foundSystemSounds) {
        AudioApplication systemApp;
        systemApp.id = "system_sounds";
        systemApp.name = "System sounds";
        systemApp.executableName = "System sounds";
        systemApp.volume = 100; // Default volume
        systemApp.isMuted = m_requestedSystemSoundsMute.value_or(false);
        systemApp.iconPath = "";
        systemApp.streamIndex = 0;
        systemApp.isSystemSounds = true;

        newApplications.append(systemApp);
    }

    m_applicationAudioLevels.clear();
    m_applications = newApplications;

    emit applicationsChanged(newApplications);
}

QImage AudioWorker::getApplicationIcon(const QString& executablePath)
{
    SHFILEINFOW info{};
    if (!SHGetFileInfoW(executablePath.toStdWString().c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON))
        return {};
    const QImage image = NativeImage::iconToImage(info.hIcon, 32);
    DestroyIcon(info.hIcon);
    return image;
}

QString AudioWorker::getApplicationDisplayName(const QString& executablePath)
{
    std::wstring wPath = executablePath.toStdWString();

    DWORD dwSize = GetFileVersionInfoSize(wPath.c_str(), NULL);
    if (dwSize == 0) {
        return QString();
    }

    std::vector<BYTE> buffer(dwSize);
    if (!GetFileVersionInfo(wPath.c_str(), 0, dwSize, buffer.data())) {
        return QString();
    }

    LPVOID lpBuffer = NULL;
    UINT uLen = 0;

    // Try ProductName first
    if (VerQueryValue(buffer.data(), L"\\StringFileInfo\\040904b0\\ProductName", &lpBuffer, &uLen) && uLen > 0) {
        return QString::fromWCharArray(static_cast<LPCWSTR>(lpBuffer));
    }

    // Try FileDescription
    if (VerQueryValue(buffer.data(), L"\\StringFileInfo\\040904b0\\FileDescription", &lpBuffer, &uLen) && uLen > 0) {
        return QString::fromWCharArray(static_cast<LPCWSTR>(lpBuffer));
    }

    return QString();
}

// Event handlers
void AudioWorker::onVolumeChanged(AudioWorker::DataFlow dataFlow, float volume, bool muted)
{
    int volumePercent = static_cast<int>(std::round(volume * 100.0f));

    if (dataFlow == Output) {
        if (m_outputVolume != volumePercent) {
            m_outputVolume = volumePercent;
            emit outputVolumeChanged(volumePercent);
        }
        if (m_outputMuted != muted) {
            m_outputMuted = muted;
            emit outputMuteChanged(muted);
        }
    } else if (dataFlow == Input) {
        if (m_inputVolume != volumePercent) {
            m_inputVolume = volumePercent;
            emit inputVolumeChanged(volumePercent);
        }
        if (m_inputMuted != muted) {
            m_inputMuted = muted;
            emit inputMuteChanged(muted);
        }
    }
}

void AudioWorker::onSessionCreated()
{
    enumerateApplications();
}

void AudioWorker::onSessionDisconnected()
{
    enumerateApplications();
}

void AudioWorker::onDeviceAdded(const QString& deviceId)
{
    enumerateDevices();
}

void AudioWorker::onDeviceRemoved(const QString& deviceId)
{
    enumerateDevices();
}

void AudioWorker::onDefaultDeviceChanged(DataFlow dataFlow, const QString& deviceId)
{
    enumerateDevices();

    // Re-setup volume notifications for the new default devices
    setupVolumeNotifications();

    // Just mark sessions as invalid for output device changes
    if (dataFlow == Output) {
        m_sessionManagerInvalid = true;
        enumerateApplications();
    }

    emit defaultDeviceChanged(deviceId, dataFlow == Input);
}

bool AudioWorker::ensureValidSessionManager()
{
    if (!m_sessionManagerInvalid && m_sessionManager) {
        return true; // Already valid
    }

    // Clean up old session manager
    if (m_sessionManager && m_sessionNotificationClient) {
        m_sessionManager->UnregisterSessionNotification(m_sessionNotificationClient);
        m_sessionNotificationClient->Release();
        m_sessionNotificationClient = nullptr;
        m_sessionManager->Release();
        m_sessionManager = nullptr;
    }

    if (!m_deviceEnumerator) {
        return false;
    }

    CComPtr<IMMDevice> device;
    HRESULT hr = m_deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        return false;
    }

    hr = device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                          nullptr, (void**)&m_sessionManager);
    if (FAILED(hr)) {
        return false;
    }

    m_sessionNotificationClient = new SessionNotificationClient(this);
    hr = m_sessionManager->RegisterSessionNotification(m_sessionNotificationClient);

    if (SUCCEEDED(hr)) {
        m_sessionManagerInvalid = false;
        return true;
    } else {
        m_sessionNotificationClient->Release();
        m_sessionNotificationClient = nullptr;
        return false;
    }
}

void AudioWorker::onApplicationSessionVolumeChanged(const QString& appId, float volume, bool muted)
{
    if (appId == "system_sounds") {
        m_requestedSystemSoundsMute = muted;
    }

    int volumePercent = static_cast<int>(std::round(volume * 100.0f));
    bool foundApp = false;
    for (int i = 0; i < m_applications.count(); ++i) {
        if (m_applications[i].id == appId) {
            foundApp = true;
            bool volumeChanged = (m_applications[i].volume != volumePercent);
            bool muteChanged = (m_applications[i].isMuted != muted);

            if (volumeChanged || muteChanged) {
                m_applications[i].volume = volumePercent;
                m_applications[i].isMuted = muted;

                if (volumeChanged) {
                    emit applicationVolumeChanged(appId, volumePercent);
                }
                if (muteChanged) {
                    emit applicationMuteChanged(appId, muted);
                }
            }
            break;
        }
    }

    if (!foundApp) {
        QMetaObject::invokeMethod(this, "enumerateApplications", Qt::QueuedConnection);
    }
}

// Control methods
void AudioWorker::setOutputVolume(int volume)
{
    setVolumeForDevice(eRender, volume);
}

void AudioWorker::setInputVolume(int volume)
{
    setVolumeForDevice(eCapture, volume);
}

void AudioWorker::setOutputMute(bool mute)
{
    setMuteForDevice(eRender, mute);
}

void AudioWorker::setInputMute(bool mute)
{
    setMuteForDevice(eCapture, mute);
}

void AudioWorker::setApplicationVolume(const QString& appId, int volume)
{
    auto it = m_sessionVolumeControls.find(appId);
    if (it != m_sessionVolumeControls.end()) {
        float volumeScalar = static_cast<float>(volume) / 100.0f;
        it.value()->SetMasterVolume(volumeScalar, nullptr);
    }
}

void AudioWorker::setApplicationMute(const QString& appId, bool mute)
{
    if (appId == "system_sounds") {
        m_requestedSystemSoundsMute = mute;
        setSystemSoundsMute(mute);
        emit applicationMuteChanged(appId, mute);
        return;
    }

    auto it = m_sessionVolumeControls.find(appId);
    if (it != m_sessionVolumeControls.end()) {
        it.value()->SetMute(mute, nullptr);
    }
}

bool AudioWorker::isSystemSoundsSession(IAudioSessionControl2* sessionControl) const
{
    if (!sessionControl) {
        return false;
    }

    HRESULT hr = sessionControl->IsSystemSoundsSession();
    return hr == S_OK;
}

bool AudioWorker::setSystemSoundsMute(bool mute)
{
    if (!ensureValidSessionManager()) {
        LOG_WARN("AudioManager", "Failed to restore session manager for system sounds mute");
        return false;
    }

    CComPtr<IAudioSessionEnumerator> sessionEnumerator;
    HRESULT hr = m_sessionManager->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr)) {
        LOG_WARN("AudioManager",
                 QString("Failed to enumerate sessions for system sounds mute, HRESULT: %1")
                     .arg(QString::number(hr, 16)));
        return false;
    }

    int sessionCount = 0;
    sessionEnumerator->GetCount(&sessionCount);

    for (int i = 0; i < sessionCount; ++i) {
        CComPtr<IAudioSessionControl> sessionControl;
        hr = sessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr) || !sessionControl) {
            continue;
        }

        CComPtr<IAudioSessionControl2> sessionControl2;
        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        if (FAILED(hr) || !isSystemSoundsSession(sessionControl2)) {
            continue;
        }

        CComPtr<ISimpleAudioVolume> volumeControl;
        hr = sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&volumeControl);
        if (FAILED(hr)) {
            LOG_WARN("AudioManager",
                     QString("Failed to query system sounds volume control, HRESULT: %1")
                         .arg(QString::number(hr, 16)));
            return false;
        }

        hr = volumeControl->SetMute(mute, nullptr);
        if (FAILED(hr)) {
            LOG_WARN("AudioManager",
                     QString("Failed to set system sounds mute, HRESULT: %1")
                         .arg(QString::number(hr, 16)));
            return false;
        }

        return true;
    }

    LOG_WARN("AudioManager", "System sounds session not found while applying mute");
    return false;
}

void AudioWorker::setDefaultDevice(const QString& deviceId, bool isInput, bool forCommunications)
{
    if (!m_policyConfig) {
        LOG_CRITICAL("AudioManager", "No policy config available, cannot set default device");
        return;
    }

    LOG_INFO("AudioManager",
             QString("Applying default %1 device switch id=%2 communicationsRole=%3")
                 .arg(isInput ? "input" : "output")
                 .arg(deviceId)
                 .arg(forCommunications ? "true" : "false"));

    std::wstring wDeviceId = deviceId.toStdWString();
    const QVector<ERole> roles = forCommunications
        ? QVector<ERole>{eCommunications}
        : QVector<ERole>{eConsole, eMultimedia};

    for (const ERole role : roles) {
        const HRESULT hr = m_policyConfig->SetDefaultEndpoint(wDeviceId.c_str(), role);
        if (!SUCCEEDED(hr)) {
            LOG_CRITICAL("AudioManager",
                         QString("Failed to set default %1 device endpoint role=%2, HRESULT: %3")
                             .arg(isInput ? "input" : "output")
                             .arg(static_cast<int>(role))
                             .arg(QString::number(hr, 16)));
        }
    }
}

void AudioWorker::setVolumeForDevice(EDataFlow dataFlow, int volume)
{
    IAudioEndpointVolume* volumeControl = (dataFlow == eRender) ? m_outputVolumeControl : m_inputVolumeControl;
    if (!volumeControl) {
        LOG_WARN("AudioManager",
                 QString("No %1 endpoint volume control available")
                     .arg(dataFlow == eRender ? "output" : "input"));
        return;
    }

    const int clampedVolume = std::clamp(volume, 0, 100);
    const float volumeScalar = static_cast<float>(clampedVolume) / 100.0f;
    const HRESULT hr = volumeControl->SetMasterVolumeLevelScalar(volumeScalar, nullptr);
    if (!SUCCEEDED(hr)) {
        LOG_CRITICAL("AudioManager",
                     QString("Failed to set %1 volume to %2, HRESULT: %3")
                         .arg(dataFlow == eRender ? "output" : "input")
                         .arg(clampedVolume)
                         .arg(QString::number(hr, 16)));
    }
}

void AudioWorker::setMuteForDevice(EDataFlow dataFlow, bool mute)
{
    IAudioEndpointVolume* volumeControl = (dataFlow == eRender) ? m_outputVolumeControl : m_inputVolumeControl;
    if (volumeControl) {
        volumeControl->SetMute(mute, nullptr);
    }
}

// AudioManager implementation
AudioManager::AudioManager(QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_cachedOutputVolume(0)
    , m_cachedInputVolume(0)
    , m_cachedOutputMute(false)
    , m_cachedInputMute(false)
{
}

AudioManager::~AudioManager()
{
    cleanup();
}

AudioManager* AudioManager::instance()
{
    QMutexLocker locker(&m_mutex);
    if (!m_instance) {
        m_instance = new AudioManager();
    }
    return m_instance;
}

void AudioManager::initialize()
{
    QMutexLocker locker(&m_mutex);
    if (m_workerThread) {
        LOG_INFO("AudioManager", "AudioManager already initialized");
        return;
    }

    if (m_retiringThread) {
        m_initializeAfterRetirement = true;
        return;
    }
    m_initializeAfterRetirement = false;

    const auto generation = ++m_generation;
    m_workerThread = new QThread(this);
    m_worker = new AudioWorker();
    if (!m_worker) {
        LOG_CRITICAL("AudioManager", "Failed to create AudioWorker");
        delete m_workerThread;
        m_workerThread = nullptr;
        return;
    }

    connect(m_worker, &AudioWorker::outputVolumeChanged,
            this, &AudioManager::onWorkerOutputVolumeChanged, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::inputVolumeChanged,
            this, &AudioManager::onWorkerInputVolumeChanged, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::outputMuteChanged,
            this, &AudioManager::onWorkerOutputMuteChanged, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::inputMuteChanged,
            this, &AudioManager::onWorkerInputMuteChanged, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::applicationsChanged,
            this, &AudioManager::onWorkerApplicationsChanged, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::devicesChanged,
            this, &AudioManager::onWorkerDevicesChanged, Qt::QueuedConnection);

    connect(
        m_worker, &AudioWorker::applicationVolumeChanged, this,
        [this, generation](const QString& id, int value) {
            if (generation == m_generation)
                emit applicationVolumeChanged(id, value);
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::applicationMuteChanged, this,
        [this, generation](const QString& id, bool value) {
            if (generation == m_generation)
                emit applicationMuteChanged(id, value);
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::deviceAdded, this,
        [this, generation](const AudioDevice& device) {
            if (generation == m_generation)
                emit deviceAdded(device);
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::deviceRemoved, this,
        [this, generation](const QString& id) {
            if (generation == m_generation)
                emit deviceRemoved(id);
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::defaultDeviceChanged, this,
        [this, generation](const QString& id, bool input) {
            if (generation == m_generation)
                emit defaultDeviceChanged(id, input);
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::initializationComplete, this,
        [this, generation]() {
            if (generation == m_generation)
                emit initializationComplete();
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::outputAudioLevelChanged, this,
        [this, generation](int value) {
            if (generation == m_generation)
                emit outputAudioLevelChanged(value);
        },
        Qt::QueuedConnection);
    connect(
        m_worker, &AudioWorker::inputAudioLevelChanged, this,
        [this, generation](int value) {
            if (generation == m_generation)
                emit inputAudioLevelChanged(value);
        },
        Qt::QueuedConnection);

    connect(
        m_worker, &AudioWorker::applicationAudioLevelChanged, this,
        [this, generation](const QString& id, int value) {
            if (generation == m_generation)
                emit applicationAudioLevelChanged(id, value);
        },
        Qt::QueuedConnection);

    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::started, m_worker, &AudioWorker::initialize);
    m_workerThread->start();
    HeadsetControlBridge::instance()->attachAudioWorker(m_worker);
}

void AudioManager::onWorkerOutputVolumeChanged(int volume)
{
    if (sender() != m_worker)
        return;
    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedOutputVolume = volume;
    cacheLock.unlock();
    emit outputVolumeChanged(volume);
}

void AudioManager::onWorkerInputVolumeChanged(int volume)
{
    if (sender() != m_worker)
        return;
    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedInputVolume = volume;
    cacheLock.unlock();
    emit inputVolumeChanged(volume);
}

void AudioManager::onWorkerOutputMuteChanged(bool muted)
{
    if (sender() != m_worker)
        return;
    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedOutputMute = muted;
    cacheLock.unlock();
    emit outputMuteChanged(muted);
}

void AudioManager::onWorkerInputMuteChanged(bool muted)
{
    if (sender() != m_worker)
        return;
    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedInputMute = muted;
    cacheLock.unlock();
    emit inputMuteChanged(muted);
}

void AudioManager::onWorkerApplicationsChanged(const QList<AudioApplication>& apps)
{
    if (sender() != m_worker)
        return;
    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedApplications = apps;
    cacheLock.unlock();
    emit applicationsChanged(apps);
}

void AudioManager::onWorkerDevicesChanged(const QList<AudioDevice>& devices)
{
    if (sender() != m_worker)
        return;
    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedDevices = devices;
    cacheLock.unlock();
    emit devicesChanged(devices);
}

void AudioManager::cleanup()
{
    QMutexLocker locker(&m_mutex);
    ++m_generation;
    m_initializeAfterRetirement = false;

    {
        QMutexLocker pendingLock(&m_pendingDefaultDeviceMutex);
        m_pendingDefaultDeviceSwitches.fill(std::nullopt);
        m_defaultDeviceSwitchDispatchQueued = false;
    }

    if (!m_workerThread) return;

    auto* worker = m_worker;
    auto* thread = m_workerThread;
        m_worker = nullptr;
    m_workerThread = nullptr;
    disconnect(worker, nullptr, this, nullptr);
    m_retiringThread = thread;
    connect(thread, &QThread::finished, this, [this, retired = QPointer<QThread>(thread)] {
        if (m_retiringThread != retired) return;
        m_retiringThread = nullptr;
        if (m_initializeAfterRetirement) initialize();
    });
    retireWorkerThread(thread, worker, "cleanup");

    QMutexLocker cacheLock(&m_cacheMutex);
    m_cachedOutputVolume = 0;
    m_cachedInputVolume = 0;
    m_cachedOutputMute = false;
    m_cachedInputMute = false;
    m_cachedApplications.clear();
    m_cachedDevices.clear();
}

void AudioManager::startApplicationAudioLevelMonitoring()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "startApplicationAudioLevelMonitoring", Qt::QueuedConnection);
    }
}

void AudioManager::stopApplicationAudioLevelMonitoring()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stopApplicationAudioLevelMonitoring", Qt::QueuedConnection);
    }
}

void AudioManager::startAudioLevelMonitoring()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "startAudioLevelMonitoring", Qt::QueuedConnection);
    }
}

void AudioManager::stopAudioLevelMonitoring()
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "stopAudioLevelMonitoring", Qt::QueuedConnection);
    }
}

// Async methods
void AudioManager::setOutputVolumeAsync(int volume)
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setOutputVolume", Qt::QueuedConnection, Q_ARG(int, volume));
    }
}

void AudioManager::setInputVolumeAsync(int volume)
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setInputVolume", Qt::QueuedConnection, Q_ARG(int, volume));
    }
}

void AudioManager::setOutputMuteAsync(bool mute)
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setOutputMute", Qt::QueuedConnection, Q_ARG(bool, mute));
    }
}

void AudioManager::setInputMuteAsync(bool mute)
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setInputMute", Qt::QueuedConnection, Q_ARG(bool, mute));
    }
}

void AudioManager::setApplicationVolumeAsync(const QString& appId, int volume)
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setApplicationVolume", Qt::QueuedConnection,
                                  Q_ARG(QString, appId), Q_ARG(int, volume));
    }
}

void AudioManager::setApplicationMuteAsync(const QString& appId, bool mute)
{
    if (m_worker) {
        QMetaObject::invokeMethod(m_worker, "setApplicationMute", Qt::QueuedConnection,
                                  Q_ARG(QString, appId), Q_ARG(bool, mute));
    }
}

void AudioManager::setDefaultDeviceAsync(const QString& deviceId, bool isInput, bool forCommunications)
{
    if (!m_worker) {
        return;
    }

    bool shouldQueueDispatcher = false;
    {
        QMutexLocker pendingLock(&m_pendingDefaultDeviceMutex);
        const int requestIndex = (isInput ? 2 : 0) + (forCommunications ? 1 : 0);
        m_pendingDefaultDeviceSwitches[requestIndex] = PendingDefaultDeviceSwitch{deviceId, isInput, forCommunications};
        if (!m_defaultDeviceSwitchDispatchQueued) {
            m_defaultDeviceSwitchDispatchQueued = true;
            shouldQueueDispatcher = true;
        }
    }

    if (!shouldQueueDispatcher) {
        return;
    }

    const bool invokeOk = QMetaObject::invokeMethod(this, &AudioManager::processPendingDefaultDeviceSwitches, Qt::QueuedConnection);
    if (!invokeOk) {
        LOG_CRITICAL("AudioManager",
                     QString("Failed to queue setDefaultDevice dispatcher for %1 device")
                         .arg(isInput ? "input" : "output"));

        QMutexLocker pendingLock(&m_pendingDefaultDeviceMutex);
        m_defaultDeviceSwitchDispatchQueued = false;
    }
}

void AudioManager::processPendingDefaultDeviceSwitches()
{
    std::array<std::optional<PendingDefaultDeviceSwitch>, 4> switchesToProcess;
    {
        QMutexLocker pendingLock(&m_pendingDefaultDeviceMutex);
        switchesToProcess = m_pendingDefaultDeviceSwitches;
        m_pendingDefaultDeviceSwitches.fill(std::nullopt);
        m_defaultDeviceSwitchDispatchQueued = false;
    }

    if (!m_worker) {
        return;
    }

    bool hadQueuedRequest = false;
    for (const auto& pendingSwitch : switchesToProcess) {
        if (!pendingSwitch.has_value()) {
            continue;
        }

        hadQueuedRequest = true;
        const PendingDefaultDeviceSwitch request = *pendingSwitch;
        const bool invokeOk = QMetaObject::invokeMethod(
            m_worker,
            [worker = m_worker, request]() {
                if (worker) {
                    worker->setDefaultDevice(request.deviceId, request.isInput, request.forCommunications);
                }
            },
            Qt::QueuedConnection);

        if (!invokeOk) {
            LOG_CRITICAL("AudioManager",
                         QString("Failed to queue setDefaultDevice execution for %1 device communicationsRole=%2")
                             .arg(request.isInput ? "input" : "output")
                             .arg(request.forCommunications ? "true" : "false"));
        }
    }

    if (!hadQueuedRequest) {
        return;
    }

    bool needsAnotherPass = false;
    {
        QMutexLocker pendingLock(&m_pendingDefaultDeviceMutex);
        for (const auto& pendingSwitch : m_pendingDefaultDeviceSwitches) {
            if (pendingSwitch.has_value() && !m_defaultDeviceSwitchDispatchQueued) {
                m_defaultDeviceSwitchDispatchQueued = true;
                needsAnotherPass = true;
                break;
            }
        }
    }

    if (needsAnotherPass) {
        QMetaObject::invokeMethod(this, &AudioManager::processPendingDefaultDeviceSwitches, Qt::QueuedConnection);
    }
}

// Cached getters
int AudioManager::getOutputVolume() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedOutputVolume;
}

int AudioManager::getInputVolume() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedInputVolume;
}

bool AudioManager::getOutputMute() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedOutputMute;
}

bool AudioManager::getInputMute() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedInputMute;
}

QList<AudioApplication> AudioManager::getApplications() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedApplications;
}

QList<AudioDevice> AudioManager::getDevices() const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cachedDevices;
}

AudioWorker* AudioManager::getWorker()
{
    return m_worker;
}

void AudioWorker::requestApplicationRefresh()
{
    if (m_applicationRefreshQueued.exchange(true))
        return;
    QMetaObject::invokeMethod(
        this,
        [this] {
            m_applicationRefreshQueued.store(false);
            if (!m_cleanedUp)
                enumerateApplications();
        },
        Qt::QueuedConnection);
}

void AudioWorker::requestDeviceRefresh()
{
    if (m_deviceRefreshQueued.exchange(true))
        return;
    QMetaObject::invokeMethod(
        this,
        [this] {
            m_deviceRefreshQueued.store(false);
            if (!m_cleanedUp)
                enumerateDevices();
        },
        Qt::QueuedConnection);
}
