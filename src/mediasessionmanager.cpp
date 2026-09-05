#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shellapi.h>
#include "mediasessionmanager.h"
#include "logmanager.h"
#include "workerthreads.h"
#include "nativeimage.h"
#include <chrono>
#include <utility>
#include <QMetaObject>
#include <QMutexLocker>
#include <QBuffer>
#include <QByteArray>

#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QSettings>
#include <winver.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <cstring>
#include <vector>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;

static QThread* g_mediaWorkerThread = nullptr;
static MediaWorker* g_mediaWorker = nullptr;
static QMutex g_mediaInitMutex;

namespace {
    template <class Operation> auto awaitResult(const Operation& operation, const std::atomic_bool& stopping)
{
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (operation.Status() == AsyncStatus::Started)
        {
            QThread::msleep(50);
            if (stopping.load() || std::chrono::steady_clock::now() >= deadline)
            {
                operation.Cancel();
                throw hresult_canceled();
            }
        }
        if (stopping.load())
        {
            operation.Cancel();
            throw hresult_canceled();
        }
        return operation.GetResults();
    }

    void queueMediaRefresh(const std::shared_ptr<MediaCallbackTarget>& target, bool resetManual = false,
                           bool playback = false)
    {
        QMutexLocker guard(&target->mutex);
        auto* worker = target->worker;
        if (!worker)
            return;
        QMetaObject::invokeMethod(
            worker,
            [target, worker, resetManual, playback] {
                {
                    QMutexLocker guard(&target->mutex);
                    if (target->worker != worker)
                        return;
                }
                worker->handleMediaEvent(resetManual, playback);
            },
            Qt::QueuedConnection);
}

    using namespace NativeImage;

QString executableDisplayName(const QString& executablePath)
{
    const std::wstring nativePath = executablePath.toStdWString();
    const DWORD dataSize = GetFileVersionInfoSize(nativePath.c_str(), nullptr);
        if (dataSize == 0)
        {
        return {};
    }

    std::vector<BYTE> versionData(dataSize);
        if (!GetFileVersionInfo(nativePath.c_str(), 0, dataSize, versionData.data()))
        {
        return {};
    }

    void* value = nullptr;
    UINT valueLength = 0;
        for (const wchar_t* key :
             {L"\\StringFileInfo\\040904b0\\ProductName", L"\\StringFileInfo\\040904b0\\FileDescription"})
        {
            if (VerQueryValue(versionData.data(), key, &value, &valueLength) && valueLength > 1)
            {
            return QString::fromWCharArray(static_cast<const wchar_t*>(value));
        }
    }
    return {};
}

QString executablePathForSource(const QString& sourceId)
{
    QString executableName = QFileInfo(sourceId).fileName();
    if (!executableName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        return {};
    }

    if (QFileInfo::exists(sourceId)) {
        return sourceId;
    }

    const QStringList registryRoots = {
        QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\"),
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\")
    };
    for (const QString& root : registryRoots) {
        QSettings registry(root + executableName, QSettings::NativeFormat);
        QString path = registry.value(QStringLiteral(".")).toString();
        path.remove(u'\"');
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    std::wstring nativeName = executableName.toStdWString();
    std::vector<wchar_t> pathBuffer(32768);
    const DWORD length = SearchPathW(nullptr, nativeName.c_str(), nullptr,
                                     static_cast<DWORD>(pathBuffer.size()), pathBuffer.data(), nullptr);
    if (length > 0 && length < pathBuffer.size()) {
        return QString::fromWCharArray(pathBuffer.data(), static_cast<qsizetype>(length));
    }
    return {};
}

void resolveSourceIdentity(const QString& sourceId, QString& sourceName, QString& sourceIcon)
{
    const std::wstring nativeId = sourceId.toStdWString();
    IShellItem* shellItem = nullptr;
    if (SUCCEEDED(SHCreateItemInKnownFolder(FOLDERID_AppsFolder, KF_FLAG_DEFAULT,
                                             nativeId.c_str(), IID_PPV_ARGS(&shellItem)))) {
        PWSTR displayName = nullptr;
        if (SUCCEEDED(shellItem->GetDisplayName(SIGDN_NORMALDISPLAY, &displayName))) {
            sourceName = QString::fromWCharArray(displayName);
            CoTaskMemFree(displayName);
        }

        IShellItemImageFactory* imageFactory = nullptr;
        if (SUCCEEDED(shellItem->QueryInterface(IID_PPV_ARGS(&imageFactory)))) {
            HBITMAP iconBitmap = nullptr;
            const SIZE iconSize{32, 32};
            if (SUCCEEDED(imageFactory->GetImage(iconSize,
                                                 static_cast<SIIGBF>(SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK),
                                                 &iconBitmap))) {
                sourceIcon = imageToDataUri(bitmapToImage(iconBitmap));
                DeleteObject(iconBitmap);
            }
            imageFactory->Release();
        }
        shellItem->Release();
    }

    const QString executablePath = executablePathForSource(sourceId);
    if (!executablePath.isEmpty()) {
        if (sourceName.isEmpty()) {
            sourceName = executableDisplayName(executablePath);
        }
        if (sourceIcon.isEmpty()) {
            SHFILEINFO fileInfo{};
            if (SHGetFileInfo(executablePath.toStdWString().c_str(), 0, &fileInfo, sizeof(fileInfo),
                              SHGFI_ICON | SHGFI_LARGEICON)) {
                sourceIcon = imageToDataUri(iconToImage(fileInfo.hIcon, 32));
                DestroyIcon(fileInfo.hIcon);
            }
        }
    }

    if (sourceName.isEmpty()) {
        sourceName = QFileInfo(sourceId).completeBaseName();
        if (sourceName.contains(u'!')) {
            sourceName = sourceName.section(u'!', -1);
        }
        if (!sourceName.isEmpty()) {
            sourceName[0] = sourceName[0].toUpper();
        }
    }
}

} // namespace

QImage createRoundedImage(const QImage& source, int targetSize, int radius)
{
    // Scale the source to target size while maintaining aspect ratio
    QImage scaled = source.scaled(targetSize, targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    // Create a new pixmap with transparent background
    QImage rounded(targetSize, targetSize, QImage::Format_ARGB32_Premultiplied);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Create a rounded rectangle path
    QPainterPath path;
    path.addRoundedRect(0, 0, targetSize, targetSize, radius, radius);

    // Set the clipping path
    painter.setClipPath(path);

    // Calculate position to center the scaled image
    int x = (targetSize - scaled.width()) / 2;
    int y = (targetSize - scaled.height()) / 2;

    // Draw the scaled image
    painter.drawImage(x, y, scaled);

    return rounded;
}

MediaInfo queryMediaInfoImpl(MediaWorker* worker) {
    MediaInfo info;

    try {
        if (!worker || !worker->ensureCurrentSession()) {
            LOG_INFO("MediaSessionManager", "No active media session found");
            return info;
        }

        auto currentSession = worker->m_currentSession;
        auto sessions = worker->m_sessionManager.GetSessions();
        info.sourceCount = static_cast<int>(sessions.Size());

        if (currentSession) {
            const QString sourceId = QString::fromWCharArray(currentSession.SourceAppUserModelId().c_str());
            resolveSourceIdentity(sourceId, info.sourceName, info.sourceIcon);

            auto properties = awaitResult(currentSession.TryGetMediaPropertiesAsync(), worker->m_stopRequested);
            if (properties) {
                info.title = QString::fromWCharArray(properties.Title().c_str());
                info.artist = QString::fromWCharArray(properties.Artist().c_str());
                info.album = QString::fromWCharArray(properties.AlbumTitle().c_str());

                LOG_INFO("MediaSessionManager",
                                                QString("Retrieved media info: %1 - %2").arg(info.artist, info.title));

                // Fetch album art
                try {
                    auto thumbnailRef = properties.Thumbnail();
                    if (thumbnailRef) {
                        auto thumbnailStream = awaitResult(thumbnailRef.OpenReadAsync(), worker->m_stopRequested);
                        if (thumbnailStream) {
                            auto size = thumbnailStream.Size();
                            if (size > 0 && size <= 8 * 1024 * 1024)
                            {
                                DataReader reader(thumbnailStream);
                                auto bytesLoaded =
                                    awaitResult(reader.LoadAsync(static_cast<uint32_t>(size)), worker->m_stopRequested);

                                if (bytesLoaded > 0) {
                                    std::vector<uint8_t> buffer(bytesLoaded);
                                    reader.ReadBytes(buffer);

                                    QByteArray originalImageData(reinterpret_cast<const char*>(buffer.data()), bytesLoaded);

                                    // Check if album art has changed using cache
                                    if (worker && originalImageData != worker->m_cachedRawAlbumArt) {
                                        LOG_INFO("MediaSessionManager", "Album art changed, processing new image");

                                        // Load and process the image only if it changed
                                        QBuffer imageBuffer(&originalImageData);
                                        imageBuffer.open(QIODevice::ReadOnly);
                                        QImageReader imageReader(&imageBuffer);
                                        const QSize imageSize = imageReader.size();
                                        QImage originalPixmap;
                                        if (imageSize.width() > 0 && imageSize.height() > 0 &&
                                            imageSize.width() <= 4096 && imageSize.height() <= 4096)
                                        {
                                            imageReader.setScaledSize(
                                                imageSize.scaled(64, 64, Qt::KeepAspectRatioByExpanding));
                                            originalPixmap = imageReader.read();
                                        }
                                        if (!originalPixmap.isNull())
                                        {
                                            int targetSize = 64;
                                            QImage roundedPixmap = createRoundedImage(originalPixmap, targetSize, 8);

                                            QByteArray processedImageData;
                                            QBuffer buffer(&processedImageData);
                                            buffer.open(QIODevice::WriteOnly);
                                            roundedPixmap.save(&buffer, "PNG");

                                            QString base64Image = processedImageData.toBase64();
                                            QString dataUri = QString("data:image/png;base64,%1").arg(base64Image);

                                            // Update cache
                                            worker->m_cachedRawAlbumArt = originalImageData;
                                            worker->m_cachedProcessedAlbumArt = dataUri;

                                            info.albumArt = dataUri;

                                            LOG_INFO("MediaSessionManager",
                                                                            QString("Album art processed successfully (%1 bytes)").arg(processedImageData.size()));
                                        }
                                    } else if (worker) {
                                        // Use cached processed album art
                                        info.albumArt = worker->m_cachedProcessedAlbumArt;
                                        LOG_INFO("MediaSessionManager", "Using cached album art");
                                    }
                                }
                            }
                        }
                    }
                }
                catch (const hresult_error& error)
                {
                    LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                                        .arg(static_cast<qint32>(error.code()), 0, 16)
                                                        .arg(QString::fromWCharArray(error.message().c_str())));
                    LOG_WARN("MediaSessionManager", "Failed to fetch album art");
                }
            }

            auto playbackInfo = currentSession.GetPlaybackInfo();
            if (playbackInfo) {
                info.isPlaying = (playbackInfo.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing);
                LOG_INFO("MediaSessionManager",
                                                QString("Playback status: %1").arg(info.isPlaying ? "Playing" : "Paused/Stopped"));
            }
        } else {
            LOG_INFO("MediaSessionManager", "No active media session found");
        }
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                            .arg(static_cast<qint32>(error.code()), 0, 16)
                                            .arg(QString::fromWCharArray(error.message().c_str())));
        LOG_CRITICAL("MediaSessionManager", "Failed to query media session info");
        worker->resetSessionManager();
        return {};
    }

    return info;
}

void MediaWorker::setupSessionManagerNotifications() {
    try {
        m_sessionManager =
            awaitResult(GlobalSystemMediaTransportControlsSessionManager::RequestAsync(), m_stopRequested);
        m_sessionsChangedToken = m_sessionManager.SessionsChanged(
            [target = m_callbackTarget](auto const&, auto const&) { queueMediaRefresh(target); });
        m_currentSessionChangedToken = m_sessionManager.CurrentSessionChanged(
            [target = m_callbackTarget](auto const&, auto const&) { queueMediaRefresh(target, true); });
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager",
                 QString("Session manager unavailable: %1").arg(QString::fromWCharArray(error.message().c_str())));
        cleanupSessionManagerNotifications();
        m_sessionManager = nullptr;
    }
}

void MediaWorker::cleanupSessionManagerNotifications() {
    if (!m_sessionManager)
        return;
    auto revoke = [&](event_token& token, auto unregister) {
        const auto previous = std::exchange(token, {});
        if (!previous.value)
            return;
        try
        {
            unregister(previous);
            }
        catch (const hresult_error& error)
        {
            LOG_WARN("MediaSessionManager", QString::fromWCharArray(error.message().c_str()));
            }
    };
    revoke(m_sessionsChangedToken, [&](auto token) { m_sessionManager.SessionsChanged(token); });
    revoke(m_currentSessionChangedToken, [&](auto token) { m_sessionManager.CurrentSessionChanged(token); });
}

bool MediaWorker::ensureCurrentSession() {
    if (!m_running || m_stopRequested.load())
        return false;
    try {
        if (!m_sessionManager) {
            setupSessionManagerNotifications();
            if (!m_sessionManager)
                return false;
        }

        auto sessions = m_sessionManager.GetSessions();
        bool currentSessionStillExists = false;
        for (const auto& session : sessions) {
            if (session == m_currentSession) {
                currentSessionStillExists = true;
                break;
            }
        }

        if (m_sourceSelectedManually && !currentSessionStillExists) {
            m_sourceSelectedManually = false;
        }

        auto currentSession = m_sourceSelectedManually
            ? m_currentSession
            : m_sessionManager.GetCurrentSession();

        // If session changed, update notifications
        if (m_currentSession != currentSession) {
            if (m_currentSession) {
                LOG_INFO("MediaSessionManager", "Media session changed, updating notifications");
            } else {
                LOG_INFO("MediaSessionManager", "New media session detected");
            }

            // Clear cache when session changes
            m_cachedRawAlbumArt.clear();
            m_cachedProcessedAlbumArt.clear();
            LOG_INFO("MediaSessionManager", "Album art cache cleared due to session change");

            cleanupSessionNotifications();
            m_currentSession = currentSession;
            if (m_currentSession) {
                setupSessionNotifications();
            }
        }

        return m_currentSession != nullptr;
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                            .arg(static_cast<qint32>(error.code()), 0, 16)
                                            .arg(QString::fromWCharArray(error.message().c_str())));
        LOG_CRITICAL("MediaSessionManager", "Failed to get current session");
        resetSessionManager();
        return false;
    }
}

void MediaWorker::setupSessionNotifications() {
    if (!m_currentSession)
        return;
    try
    {
        m_propertiesChangedToken = m_currentSession.MediaPropertiesChanged(
            [target = m_callbackTarget](auto const&, auto const&) { queueMediaRefresh(target); });
        m_playbackInfoChangedToken = m_currentSession.PlaybackInfoChanged(
            [target = m_callbackTarget](auto const&, auto const&) { queueMediaRefresh(target, false, true); });
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager",
                 QString("Session notifications failed: %1").arg(QString::fromWCharArray(error.message().c_str())));
        cleanupSessionNotifications();
    }
}

void MediaWorker::cleanupSessionNotifications() {
    if (!m_currentSession)
        return;
    auto revoke = [&](event_token& token, auto unregister) {
        const auto previous = std::exchange(token, {});
        if (!previous.value)
            return;
        try
        {
            unregister(previous);
            }
        catch (const hresult_error& error)
        {
            LOG_WARN("MediaSessionManager", QString::fromWCharArray(error.message().c_str()));
            }
    };
    revoke(m_propertiesChangedToken, [&](auto token) { m_currentSession.MediaPropertiesChanged(token); });
    revoke(m_playbackInfoChangedToken, [&](auto token) { m_currentSession.PlaybackInfoChanged(token); });
        }

MediaWorker::MediaWorker()
{
    qRegisterMetaType<MediaInfo>();
    m_callbackTarget->worker = this;
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &MediaWorker::queryMediaInfo);
}
MediaWorker::~MediaWorker()
{
    cleanup();
}
void MediaWorker::cleanup()
{
    {
        QMutexLocker guard(&m_callbackTarget->mutex);
        m_callbackTarget->worker = nullptr;
    }
    stopMonitoring();
    if (m_apartmentInitialized)
    {
        uninit_apartment();
        m_apartmentInitialized = false;
    }
}

void MediaWorker::queryMediaInfo() {
    if (!m_running || m_stopRequested.load())
        return;
    MediaInfo info = queryMediaInfoImpl(this);
    if (!m_sessionManager && !m_stopRequested.load())
    {
        m_retryTimer->start(m_retryInterval);
        m_retryInterval = qMin(m_retryInterval * 2, 60000);
    }
    else
        m_retryInterval = 2000;
    if (m_running && !m_stopRequested.load())
    emit mediaInfoChanged(info);
}

void MediaWorker::startMonitoring() {
    if (m_running && !m_stopRequested.load())
        return;
    if (m_running)
        stopMonitoring();
    m_callbackTarget = std::make_shared<MediaCallbackTarget>();
    m_callbackTarget->worker = this;
    m_stopRequested.store(false);
    try
    {
        if (!m_apartmentInitialized)
        {
            init_apartment(apartment_type::multi_threaded);
            m_apartmentInitialized = true;
        }
    }
    catch (const hresult_error& error)
    {
        LOG_CRITICAL("MediaSessionManager", QString::fromWCharArray(error.message().c_str()));
        return;
    }
    m_running = true;
    LOG_INFO("MediaSessionManager", "Starting media session monitoring");

    // Clear cache on start
    m_cachedRawAlbumArt.clear();
    m_cachedProcessedAlbumArt.clear();

    setupSessionManagerNotifications();
    ensureCurrentSession();
    queryMediaInfo();

    LOG_INFO("MediaSessionManager", "Media monitoring started (fully event-driven)");
}

void MediaWorker::stopMonitoring() {
    {
        QMutexLocker guard(&m_callbackTarget->mutex);
        m_callbackTarget->worker = nullptr;
    }
    m_retryTimer->stop();
    m_running = false;
    m_stopRequested.store(true);
    emit mediaInfoChanged(MediaInfo{});
    LOG_INFO("MediaSessionManager", "Stopping media session monitoring");

    resetSessionManager();

    // Clear cache on stop
    m_cachedRawAlbumArt.clear();
    m_cachedProcessedAlbumArt.clear();

    LOG_INFO("MediaSessionManager", "Media monitoring stopped");
}

void MediaWorker::playPause() {
    LOG_INFO("MediaSessionManager", "Toggling play/pause");

    try {
        if (ensureCurrentSession() && m_currentSession) {
            if (!awaitResult(m_currentSession.TryTogglePlayPauseAsync(), m_stopRequested))
            {
                LOG_WARN("MediaSessionManager", "Media source rejected transport command");
                return;
            }
            LOG_INFO("MediaSessionManager", "Play/pause toggled successfully");
            // Event will trigger automatically via PlaybackInfoChanged
        } else {
            LOG_WARN("MediaSessionManager", "No active session for play/pause toggle");
        }
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                            .arg(static_cast<qint32>(error.code()), 0, 16)
                                            .arg(QString::fromWCharArray(error.message().c_str())));
        LOG_CRITICAL("MediaSessionManager", "Failed to toggle play/pause");
    }
}

void MediaWorker::nextTrack() {
    LOG_INFO("MediaSessionManager", "Skipping to next track");

    try {
        if (ensureCurrentSession() && m_currentSession) {
            if (!awaitResult(m_currentSession.TrySkipNextAsync(), m_stopRequested))
            {
                LOG_WARN("MediaSessionManager", "Media source rejected transport command");
                return;
            }
            LOG_INFO("MediaSessionManager", "Successfully skipped to next track");
            // Event will trigger automatically via MediaPropertiesChanged
        } else {
            LOG_WARN("MediaSessionManager", "No active session for next track");
        }
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                            .arg(static_cast<qint32>(error.code()), 0, 16)
                                            .arg(QString::fromWCharArray(error.message().c_str())));
        LOG_CRITICAL("MediaSessionManager", "Failed to skip to next track");
    }
}

void MediaWorker::previousTrack() {
    LOG_INFO("MediaSessionManager", "Skipping to previous track");

    try {
        if (ensureCurrentSession() && m_currentSession) {
            if (!awaitResult(m_currentSession.TrySkipPreviousAsync(), m_stopRequested))
            {
                LOG_WARN("MediaSessionManager", "Media source rejected transport command");
                return;
            }
            LOG_INFO("MediaSessionManager", "Successfully skipped to previous track");
            // Event will trigger automatically via MediaPropertiesChanged
        } else {
            LOG_WARN("MediaSessionManager", "No active session for previous track");
        }
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                            .arg(static_cast<qint32>(error.code()), 0, 16)
                                            .arg(QString::fromWCharArray(error.message().c_str())));
        LOG_CRITICAL("MediaSessionManager", "Failed to skip to previous track");
    }
}

void MediaWorker::nextSource() {
    LOG_INFO("MediaSessionManager", "Switching to next media source");

    try {
        if (!ensureCurrentSession()) {
            return;
        }

        const auto sessions = m_sessionManager.GetSessions();
        if (sessions.Size() < 2) {
            return;
        }

        uint32_t currentIndex = 0;
        for (uint32_t index = 0; index < sessions.Size(); ++index) {
            if (sessions.GetAt(index) == m_currentSession) {
                currentIndex = index;
                break;
            }
        }

        cleanupSessionNotifications();
        m_currentSession = sessions.GetAt((currentIndex + 1) % sessions.Size());
        m_sourceSelectedManually = true;
        m_cachedRawAlbumArt.clear();
        m_cachedProcessedAlbumArt.clear();
        setupSessionNotifications();
        queryMediaInfo();
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString("WinRT error %1: %2")
                                            .arg(static_cast<qint32>(error.code()), 0, 16)
                                            .arg(QString::fromWCharArray(error.message().c_str())));
        LOG_CRITICAL("MediaSessionManager", "Failed to switch media source");
    }
}

void MediaSessionManager::initialize() {
    QMutexLocker locker(&g_mediaInitMutex);

    LOG_INFO("MediaSessionManager", "Initializing MediaSessionManager");

    if (!g_mediaWorkerThread) {
        g_mediaWorkerThread = new QThread;
        g_mediaWorker = new MediaWorker;
        g_mediaWorker->moveToThread(g_mediaWorkerThread);
        g_mediaWorkerThread->start();

        LOG_INFO("MediaSessionManager", "MediaSessionManager worker thread started");
    } else {
        LOG_INFO("MediaSessionManager", "MediaSessionManager already initialized");
    }
}

void MediaSessionManager::cleanup() {
    QMutexLocker locker(&g_mediaInitMutex);
    if (!g_mediaWorkerThread)
        return;
    g_mediaWorker->requestStop();
    retireWorkerThread(g_mediaWorkerThread, g_mediaWorker, "cleanup");
        g_mediaWorker = nullptr;
        g_mediaWorkerThread = nullptr;
}

void MediaSessionManager::queryMediaInfoAsync() {
    if (g_mediaWorker) {
        QMetaObject::invokeMethod(g_mediaWorker, "queryMediaInfo", Qt::QueuedConnection);
    }
}

void MediaSessionManager::startMonitoringAsync() {
    if (g_mediaWorker) {
        QMetaObject::invokeMethod(g_mediaWorker, "startMonitoring", Qt::QueuedConnection);
    }
}

void MediaSessionManager::stopMonitoringAsync() {
    if (g_mediaWorker) {
        g_mediaWorker->requestStop();
        QMetaObject::invokeMethod(g_mediaWorker, "stopMonitoring", Qt::QueuedConnection);
    }
}

void MediaSessionManager::playPauseAsync() {
    if (g_mediaWorker) {
        QMetaObject::invokeMethod(g_mediaWorker, "playPause", Qt::QueuedConnection);
    }
}

void MediaSessionManager::nextTrackAsync() {
    if (g_mediaWorker) {
        QMetaObject::invokeMethod(g_mediaWorker, "nextTrack", Qt::QueuedConnection);
    }
}

void MediaSessionManager::previousTrackAsync() {
    if (g_mediaWorker) {
        QMetaObject::invokeMethod(g_mediaWorker, "previousTrack", Qt::QueuedConnection);
    }
}

void MediaSessionManager::nextSourceAsync() {
    if (g_mediaWorker) {
        QMetaObject::invokeMethod(g_mediaWorker, "nextSource", Qt::QueuedConnection);
    }
}

MediaWorker* MediaSessionManager::getWorker() {
    return g_mediaWorker;
}

void MediaWorker::handleMediaEvent(bool resetManualSelection, bool checkPlayback)
{
    if (!m_running || m_stopRequested.load())
        return;
    try
    {
        if (resetManualSelection)
            m_sourceSelectedManually = false;
        if (checkPlayback && m_sourceSelectedManually && m_currentSession)
        {
            const auto info = m_currentSession.GetPlaybackInfo();
            if (info && (info.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped ||
                         info.PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed))
            {
                m_sourceSelectedManually = false;
            }
        }
    }
    catch (const hresult_error& error)
    {
        LOG_WARN("MediaSessionManager", QString::fromWCharArray(error.message().c_str()));
    }
    queryMediaInfo();
}

void MediaWorker::resetSessionManager()
{
    cleanupSessionNotifications();
    cleanupSessionManagerNotifications();
    m_currentSession = nullptr;
    m_sessionManager = nullptr;
    m_sourceSelectedManually = false;
}
