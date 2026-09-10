#include "mediasessionbridge.h"
#include "mediasessionmanager.h"
#include "usersettings.h"

MediaSessionBridge* MediaSessionBridge::m_instance = nullptr;

MediaSessionBridge::MediaSessionBridge(QObject* parent)
    : QObject(parent)
{
    m_instance = this;
    MediaSessionManager::initialize();
    if (MediaSessionManager::getWorker()) {
        connect(MediaSessionManager::getWorker(), &MediaWorker::mediaInfoChanged,
                this, [this](const MediaInfo& info) {
                    m_mediaTitle = info.title;
                    m_mediaArtist = info.artist;
                    m_mediaArt = info.albumArt;
                    m_isMediaPlaying = info.isPlaying;
                    m_sourceName = info.sourceName;
                    m_sourceIcon = info.sourceIcon;
                    m_sourceCount = info.sourceCount;
                    m_canPreviousTrack = info.canPreviousTrack;
                    m_hasMediaTimeline = info.hasMediaTimeline;
                    m_canSeek = info.canSeek;
                    m_mediaPositionMs = info.mediaPositionMs;
                    m_mediaDurationMs = info.mediaDurationMs;
                    m_mediaMinimumSeekMs = info.mediaMinimumSeekMs;
                    m_mediaMaximumSeekMs = info.mediaMaximumSeekMs;
                    m_mediaPlaybackRate = info.mediaPlaybackRate;
                    emit mediaInfoChanged();
                });
    }

    connect(UserSettings::instance(), &UserSettings::enableMediaSessionManagerChanged, this, [this] {
        if (UserSettings::instance()->enableMediaSessionManager())
            startMediaMonitoring();
        else
            stopMediaMonitoring();
    });
    if (UserSettings::instance()->enableMediaSessionManager()) {
        startMediaMonitoring();
    }
}

MediaSessionBridge::~MediaSessionBridge()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}

MediaSessionBridge* MediaSessionBridge::create(QQmlEngine* qmlEngine, QJSEngine* jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)

    if (!m_instance) {
        m_instance = new MediaSessionBridge();
    }
    return m_instance;
}

MediaSessionBridge* MediaSessionBridge::instance()
{
    return m_instance;
}

QString MediaSessionBridge::mediaTitle() const {
    return m_mediaTitle;
}

QString MediaSessionBridge::mediaArtist() const {
    return m_mediaArtist;
}

bool MediaSessionBridge::isMediaPlaying() const {
    return m_isMediaPlaying;
}

QString MediaSessionBridge::mediaArt() const {
    return m_mediaArt;
}

QString MediaSessionBridge::sourceName() const {
    return m_sourceName;
}

QString MediaSessionBridge::sourceIcon() const {
    return m_sourceIcon;
}

int MediaSessionBridge::sourceCount() const {
    return m_sourceCount;
}

bool MediaSessionBridge::canPreviousTrack() const {
    return m_canPreviousTrack;
}

bool MediaSessionBridge::hasMediaTimeline() const {
    return m_hasMediaTimeline;
}

bool MediaSessionBridge::canSeek() const {
    return m_canSeek;
}

qint64 MediaSessionBridge::mediaPositionMs() const {
    return m_mediaPositionMs;
}

qint64 MediaSessionBridge::mediaDurationMs() const {
    return m_mediaDurationMs;
}

qint64 MediaSessionBridge::mediaMinimumSeekMs() const {
    return m_mediaMinimumSeekMs;
}

qint64 MediaSessionBridge::mediaMaximumSeekMs() const {
    return m_mediaMaximumSeekMs;
}

double MediaSessionBridge::mediaPlaybackRate() const {
    return m_mediaPlaybackRate;
}

void MediaSessionBridge::playPause() {
    MediaSessionManager::playPauseAsync();
}

void MediaSessionBridge::nextTrack() {
    MediaSessionManager::nextTrackAsync();
}

void MediaSessionBridge::previousTrack() {
    MediaSessionManager::previousTrackAsync();
}

void MediaSessionBridge::seekTo(qint64 positionMs) {
    MediaSessionManager::seekToAsync(positionMs);
}

void MediaSessionBridge::nextSource() {
    emit mediaSourceSwitchRequested();
    MediaSessionManager::nextSourceAsync();
}

void MediaSessionBridge::startMediaMonitoring() {
    MediaSessionManager::startMonitoringAsync();
}

void MediaSessionBridge::stopMediaMonitoring() {
    MediaSessionManager::stopMonitoringAsync();
}
