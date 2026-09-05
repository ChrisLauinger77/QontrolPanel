#pragma once
#include <QObject>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QTimer>
#include <QByteArray>
#include <atomic>
#include <memory>
#include <windows.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;
using namespace Windows::Media::Control;
using namespace Windows::Foundation;

struct MediaInfo {
    QString title;
    QString artist;
    QString album;
    bool isPlaying = false;
    QString albumArt;
    QString sourceName;
    QString sourceIcon;
    int sourceCount = 0;
};

Q_DECLARE_METATYPE(MediaInfo)

class MediaWorker;
struct MediaCallbackTarget
{
    QMutex mutex;
    MediaWorker* worker = nullptr;
};

class MediaWorker : public QObject
{
    Q_OBJECT

public:
    MediaWorker();
    ~MediaWorker() override;
    void requestStop() { m_stopRequested.store(true); }
public slots:
    void cleanup();
    void queryMediaInfo();
    void handleMediaEvent(bool resetManualSelection, bool checkPlayback);
    void startMonitoring();
    void stopMonitoring();
    void playPause();
    void nextTrack();
    void previousTrack();
    void nextSource();

signals:
    void mediaInfoChanged(const MediaInfo& info);

private:
    QTimer* m_retryTimer = nullptr;
    int m_retryInterval = 2000;
    bool m_running = false;
    bool m_apartmentInitialized = false;
    std::atomic_bool m_stopRequested{false};
    std::shared_ptr<MediaCallbackTarget> m_callbackTarget = std::make_shared<MediaCallbackTarget>();
    GlobalSystemMediaTransportControlsSessionManager m_sessionManager{nullptr};
    GlobalSystemMediaTransportControlsSession m_currentSession{nullptr};
    bool m_sourceSelectedManually = false;

    // Event tokens for cleanup
    event_token m_sessionsChangedToken{};
    event_token m_currentSessionChangedToken{};
    event_token m_propertiesChangedToken{};
    event_token m_playbackInfoChangedToken{};

    // Cache for album art to avoid reprocessing
    QByteArray m_cachedRawAlbumArt;
    QString m_cachedProcessedAlbumArt;

    void resetSessionManager();
    void setupSessionManagerNotifications();
    void cleanupSessionManagerNotifications();
    void setupSessionNotifications();
    void cleanupSessionNotifications();
    bool ensureCurrentSession();

    friend MediaInfo queryMediaInfoImpl(MediaWorker* worker);
};

namespace MediaSessionManager
{
void initialize();
void cleanup();
void queryMediaInfoAsync();
void startMonitoringAsync();
void stopMonitoringAsync();
void playPauseAsync();
void nextTrackAsync();
void previousTrackAsync();
void nextSourceAsync();
MediaWorker* getWorker();
}
