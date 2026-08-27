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

public:
    explicit MediaSessionBridge(QObject* parent = nullptr);
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

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void nextTrack();
    Q_INVOKABLE void previousTrack();
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
};

