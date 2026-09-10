#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

class MediaSessionBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString mediaArtist READ mediaArtist NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool isMediaPlaying READ isMediaPlaying NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString mediaArt READ mediaArt NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY mediaInfoChanged)
    Q_PROPERTY(QString sourceIcon READ sourceIcon NOTIFY mediaInfoChanged)
    Q_PROPERTY(int sourceCount READ sourceCount NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool canPreviousTrack READ canPreviousTrack NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool hasMediaTimeline READ hasMediaTimeline NOTIFY mediaInfoChanged)
    Q_PROPERTY(bool canSeek READ canSeek NOTIFY mediaInfoChanged)
    Q_PROPERTY(qint64 mediaPositionMs READ mediaPositionMs NOTIFY mediaInfoChanged)
    Q_PROPERTY(qint64 mediaDurationMs READ mediaDurationMs NOTIFY mediaInfoChanged)
    Q_PROPERTY(qint64 mediaMinimumSeekMs READ mediaMinimumSeekMs NOTIFY mediaInfoChanged)
    Q_PROPERTY(qint64 mediaMaximumSeekMs READ mediaMaximumSeekMs NOTIFY mediaInfoChanged)
    Q_PROPERTY(double mediaPlaybackRate READ mediaPlaybackRate NOTIFY mediaInfoChanged)

private:
    explicit MediaSessionBridge(QObject* parent = nullptr);

public:
    ~MediaSessionBridge() override;

    static MediaSessionBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);
    static MediaSessionBridge* instance();

    QString mediaTitle() const;
    QString mediaArtist() const;
    bool isMediaPlaying() const;
    QString mediaArt() const;
    QString sourceName() const;
    QString sourceIcon() const;
    int sourceCount() const;
    bool canPreviousTrack() const;
    bool hasMediaTimeline() const;
    bool canSeek() const;
    qint64 mediaPositionMs() const;
    qint64 mediaDurationMs() const;
    qint64 mediaMinimumSeekMs() const;
    qint64 mediaMaximumSeekMs() const;
    double mediaPlaybackRate() const;

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void nextTrack();
    Q_INVOKABLE void previousTrack();
    Q_INVOKABLE void seekTo(qint64 positionMs);
    Q_INVOKABLE void nextSource();
    Q_INVOKABLE void startMediaMonitoring();
    Q_INVOKABLE void stopMediaMonitoring();

signals:
    void mediaInfoChanged();
    void mediaSourceSwitchRequested();

private:
    static MediaSessionBridge* m_instance;

    QString m_mediaTitle;
    QString m_mediaArtist;
    bool m_isMediaPlaying = false;
    QString m_mediaArt;
    QString m_sourceName;
    QString m_sourceIcon;
    int m_sourceCount = 0;
    bool m_canPreviousTrack = false;
    bool m_hasMediaTimeline = false;
    bool m_canSeek = false;
    qint64 m_mediaPositionMs = 0;
    qint64 m_mediaDurationMs = 0;
    qint64 m_mediaMinimumSeekMs = 0;
    qint64 m_mediaMaximumSeekMs = 0;
    double m_mediaPlaybackRate = 1.0;
};
