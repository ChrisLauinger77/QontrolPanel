#include "mediasessionbridge.h"
#include "mediasessionmanager.h"
#include "usersettings.h"

MediaSessionBridge* MediaSessionBridge::m_instance = nullptr;

MediaSessionBridge::MediaSessionBridge(QObject* parent)
    : QObject(parent)
{
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
                    emit mediaInfoChanged();
                });
    }

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

void MediaSessionBridge::playPause() {
    MediaSessionManager::playPauseAsync();
}

void MediaSessionBridge::nextTrack() {
    MediaSessionManager::nextTrackAsync();
}

void MediaSessionBridge::previousTrack() {
    MediaSessionManager::previousTrackAsync();
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
