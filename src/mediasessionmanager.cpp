#include <windows.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <shellapi.h>
#include "mediasessionmanager.h"
#include "logmanager.h"
#include <QMetaObject>
#include <QMutexLocker>
#include <QBuffer>
#include <QByteArray>
#include <QPixmap>
#include <QImage>
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

QString imageToDataUri(const QImage& image)
{
    if (image.isNull()) {
        return {};
    }

    QByteArray imageData;
    QBuffer buffer(&imageData);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + imageData.toBase64();
}

QImage bitmapToImage(HBITMAP bitmap)
{
    BITMAP bitmapInfo{};
    if (!bitmap || GetObject(bitmap, sizeof(bitmapInfo), &bitmapInfo) == 0) {
        return {};
    }

    BITMAPINFO dibInfo{};
    dibInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    dibInfo.bmiHeader.biWidth = bitmapInfo.bmWidth;
    dibInfo.bmiHeader.biHeight = -bitmapInfo.bmHeight;
    dibInfo.bmiHeader.biPlanes = 1;
    dibInfo.bmiHeader.biBitCount = 32;
    dibInfo.bmiHeader.biCompression = BI_RGB;

    QImage image(bitmapInfo.bmWidth, bitmapInfo.bmHeight, QImage::Format_ARGB32);
    HDC deviceContext = GetDC(nullptr);
    const int copiedLines = GetDIBits(deviceContext, bitmap, 0, bitmapInfo.bmHeight,
                                      image.bits(), &dibInfo, DIB_RGB_COLORS);
    ReleaseDC(nullptr, deviceContext);

    return copiedLines == bitmapInfo.bmHeight ? image : QImage{};
}

QImage iconToImage(HICON icon, int size)
{
    if (!icon) {
        return {};
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = size;
    bitmapInfo.bmiHeader.biHeight = -size;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HDC screenContext = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(screenContext, &bitmapInfo, DIB_RGB_COLORS,
                                      &pixels, nullptr, 0);
    HDC drawContext = CreateCompatibleDC(screenContext);
    if (!screenContext || !bitmap || !pixels || !drawContext) {
        if (drawContext) {
            DeleteDC(drawContext);
        }
        if (bitmap) {
            DeleteObject(bitmap);
        }
        if (screenContext) {
            ReleaseDC(nullptr, screenContext);
        }
        return {};
    }

    HGDIOBJ oldBitmap = SelectObject(drawContext, bitmap);
    memset(pixels, 0, static_cast<size_t>(size * size * 4));
    DrawIconEx(drawContext, 0, 0, icon, size, size, 0, nullptr, DI_NORMAL);

    QImage image(static_cast<uchar*>(pixels), size, size, QImage::Format_ARGB32);
    QImage result = image.copy();

    SelectObject(drawContext, oldBitmap);
    DeleteDC(drawContext);
    DeleteObject(bitmap);
    ReleaseDC(nullptr, screenContext);
    return result;
}

QString executableDisplayName(const QString& executablePath)
{
    const std::wstring nativePath = executablePath.toStdWString();
    const DWORD dataSize = GetFileVersionInfoSize(nativePath.c_str(), nullptr);
    if (dataSize == 0) {
        return {};
    }

    std::vector<BYTE> versionData(dataSize);
    if (!GetFileVersionInfo(nativePath.c_str(), 0, dataSize, versionData.data())) {
        return {};
    }

    void* value = nullptr;
    UINT valueLength = 0;
    for (const wchar_t* key : {L"\\StringFileInfo\\040904b0\\ProductName",
                               L"\\StringFileInfo\\040904b0\\FileDescription"}) {
        if (VerQueryValue(versionData.data(), key, &value, &valueLength) && valueLength > 1) {
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

QPixmap createRoundedPixmap(const QPixmap& source, int targetSize, int radius) {
    // Scale the source to target size while maintaining aspect ratio
    QPixmap scaled = source.scaled(targetSize, targetSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);

    // Create a new pixmap with transparent background
    QPixmap rounded(targetSize, targetSize);
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
    painter.drawPixmap(x, y, scaled);

    return rounded;
}

MediaInfo queryMediaInfoImpl(MediaWorker* worker) {
    MediaInfo info;

    try {
        init_apartment();
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

            auto properties = currentSession.TryGetMediaPropertiesAsync().get();
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
                        auto thumbnailStream = thumbnailRef.OpenReadAsync().get();
                        if (thumbnailStream) {
                            auto size = thumbnailStream.Size();
                            if (size > 0) {
                                DataReader reader(thumbnailStream);
                                auto bytesLoaded = reader.LoadAsync(static_cast<uint32_t>(size)).get();

                                if (bytesLoaded > 0) {
                                    std::vector<uint8_t> buffer(bytesLoaded);
                                    reader.ReadBytes(buffer);

                                    QByteArray originalImageData(reinterpret_cast<const char*>(buffer.data()), bytesLoaded);

                                    // Check if album art has changed using cache
                                    if (worker && originalImageData != worker->m_cachedRawAlbumArt) {
                                        LOG_INFO("MediaSessionManager", "Album art changed, processing new image");

                                        // Load and process the image only if it changed
                                        QPixmap originalPixmap;
                                        if (originalPixmap.loadFromData(originalImageData)) {
                                            int targetSize = 64;
                                            QPixmap roundedPixmap = createRoundedPixmap(originalPixmap, targetSize, 8);

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
                } catch (...) {
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
    } catch (...) {
        LOG_CRITICAL("MediaSessionManager", "Failed to query media session info");
    }

    return info;
}

void MediaWorker::setupSessionManagerNotifications() {
    try {
        init_apartment();
        m_sessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

        // Listen for when sessions are added/removed
        m_sessionsChangedToken = m_sessionManager.SessionsChanged(
            [this](GlobalSystemMediaTransportControlsSessionManager const& sender, SessionsChangedEventArgs const& args) {
                Q_UNUSED(sender)
                Q_UNUSED(args)
                QMetaObject::invokeMethod(this, [this]() {
                    LOG_INFO("MediaSessionManager", "Sessions changed, checking for new active session");
                    ensureCurrentSession();
                    queryMediaInfo();
                }, Qt::QueuedConnection);
            });

        // Follow the source Windows considers most relevant, matching Quick Settings.
        m_currentSessionChangedToken = m_sessionManager.CurrentSessionChanged(
            [this](GlobalSystemMediaTransportControlsSessionManager const& sender,
                   CurrentSessionChangedEventArgs const& args) {
                Q_UNUSED(sender)
                Q_UNUSED(args)
                QMetaObject::invokeMethod(this, [this]() {
                    LOG_INFO("MediaSessionManager", "Current media session changed");
                    m_sourceSelectedManually = false;
                    queryMediaInfo();
                }, Qt::QueuedConnection);
            });

        LOG_INFO("MediaSessionManager", "Session manager notifications registered");
    } catch (...) {
        LOG_CRITICAL("MediaSessionManager", "Failed to setup session manager notifications");
    }
}

void MediaWorker::cleanupSessionManagerNotifications() {
    if (m_sessionManager) {
        try {
            if (m_sessionsChangedToken.value != 0) {
                m_sessionManager.SessionsChanged(m_sessionsChangedToken);
                m_sessionsChangedToken = {};
            }
            if (m_currentSessionChangedToken.value != 0) {
                m_sessionManager.CurrentSessionChanged(m_currentSessionChangedToken);
                m_currentSessionChangedToken = {};
            }
            LOG_INFO("MediaSessionManager", "Session manager notifications cleaned up");
        } catch (...) {
            LOG_WARN("MediaSessionManager", "Error cleaning up session manager notifications");
        }
    }
}

bool MediaWorker::ensureCurrentSession() {
    try {
        if (!m_sessionManager) {
            init_apartment();
            m_sessionManager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();
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
    } catch (...) {
        LOG_CRITICAL("MediaSessionManager", "Failed to get current session");
        return false;
    }
}

void MediaWorker::setupSessionNotifications() {
    if (!m_currentSession) {
        return;
    }

    LOG_INFO("MediaSessionManager", "Setting up session event notifications");

    try {
        // Register for media properties changes (title, artist, album art)
        m_propertiesChangedToken = m_currentSession.MediaPropertiesChanged(
            [this](GlobalSystemMediaTransportControlsSession const& session,
                   MediaPropertiesChangedEventArgs const& args) {
                Q_UNUSED(session)
                Q_UNUSED(args)
                QMetaObject::invokeMethod(this, "queryMediaInfo", Qt::QueuedConnection);
            });

        // Register for playback info changes (play/pause state)
        m_playbackInfoChangedToken = m_currentSession.PlaybackInfoChanged(
            [this](GlobalSystemMediaTransportControlsSession const& session,
                   PlaybackInfoChangedEventArgs const& args) {
                Q_UNUSED(session)
                Q_UNUSED(args)
                QMetaObject::invokeMethod(this, [this]() {
                    if (m_sourceSelectedManually && m_currentSession) {
                        const auto playbackInfo = m_currentSession.GetPlaybackInfo();
                        if (playbackInfo) {
                            const auto status = playbackInfo.PlaybackStatus();
                            if (status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped
                                || status == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed) {
                                m_sourceSelectedManually = false;
                            }
                        }
                    }
                    queryMediaInfo();
                }, Qt::QueuedConnection);
            });

        LOG_INFO("MediaSessionManager", "Session event notifications registered successfully");
    } catch (...) {
        LOG_CRITICAL("MediaSessionManager", "Failed to register session notifications");
    }
}

void MediaWorker::cleanupSessionNotifications() {
    if (m_currentSession) {
        LOG_INFO("MediaSessionManager", "Cleaning up session notifications");
        try {
            if (m_propertiesChangedToken.value != 0) {
                m_currentSession.MediaPropertiesChanged(m_propertiesChangedToken);
                m_propertiesChangedToken = {};
            }
            if (m_playbackInfoChangedToken.value != 0) {
                m_currentSession.PlaybackInfoChanged(m_playbackInfoChangedToken);
                m_playbackInfoChangedToken = {};
            }
            LOG_INFO("MediaSessionManager", "Session notifications cleaned up successfully");
        } catch (...) {
            LOG_WARN("MediaSessionManager", "Error cleaning up session notifications");
        }
    }
}

void MediaWorker::queryMediaInfo() {
    MediaInfo info = queryMediaInfoImpl(this);
    emit mediaInfoChanged(info);
}

void MediaWorker::startMonitoring() {
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
    LOG_INFO("MediaSessionManager", "Stopping media session monitoring");

    cleanupSessionNotifications();
    cleanupSessionManagerNotifications();
    m_currentSession = nullptr;
    m_sessionManager = nullptr;
    m_sourceSelectedManually = false;

    // Clear cache on stop
    m_cachedRawAlbumArt.clear();
    m_cachedProcessedAlbumArt.clear();

    LOG_INFO("MediaSessionManager", "Media monitoring stopped");
}

void MediaWorker::playPause() {
    LOG_INFO("MediaSessionManager", "Toggling play/pause");

    try {
        if (ensureCurrentSession() && m_currentSession) {
            m_currentSession.TryTogglePlayPauseAsync().get();
            LOG_INFO("MediaSessionManager", "Play/pause toggled successfully");
            // Event will trigger automatically via PlaybackInfoChanged
        } else {
            LOG_WARN("MediaSessionManager", "No active session for play/pause toggle");
        }
    } catch (...) {
        LOG_CRITICAL("MediaSessionManager", "Failed to toggle play/pause");
    }
}

void MediaWorker::nextTrack() {
    LOG_INFO("MediaSessionManager", "Skipping to next track");

    try {
        if (ensureCurrentSession() && m_currentSession) {
            m_currentSession.TrySkipNextAsync().get();
            LOG_INFO("MediaSessionManager", "Successfully skipped to next track");
            // Event will trigger automatically via MediaPropertiesChanged
        } else {
            LOG_WARN("MediaSessionManager", "No active session for next track");
        }
    } catch (...) {
        LOG_CRITICAL("MediaSessionManager", "Failed to skip to next track");
    }
}

void MediaWorker::previousTrack() {
    LOG_INFO("MediaSessionManager", "Skipping to previous track");

    try {
        if (ensureCurrentSession() && m_currentSession) {
            m_currentSession.TrySkipPreviousAsync().get();
            LOG_INFO("MediaSessionManager", "Successfully skipped to previous track");
            // Event will trigger automatically via MediaPropertiesChanged
        } else {
            LOG_WARN("MediaSessionManager", "No active session for previous track");
        }
    } catch (...) {
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
    } catch (...) {
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

    LOG_INFO("MediaSessionManager", "Cleaning up MediaSessionManager");

    if (g_mediaWorkerThread) {
        // Stop monitoring and cleanup notifications before quitting thread
        if (g_mediaWorker) {
            QMetaObject::invokeMethod(g_mediaWorker, "stopMonitoring", Qt::BlockingQueuedConnection);
        }

        g_mediaWorkerThread->quit();
        if (!g_mediaWorkerThread->wait(3000)) {
            LOG_WARN("MediaSessionManager", "Worker thread did not quit gracefully, terminating");
            g_mediaWorkerThread->terminate();
            g_mediaWorkerThread->wait(1000);
        }

        delete g_mediaWorker;
        delete g_mediaWorkerThread;
        g_mediaWorker = nullptr;
        g_mediaWorkerThread = nullptr;

        LOG_INFO("MediaSessionManager", "MediaSessionManager cleanup complete");
    }
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
