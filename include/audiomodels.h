#pragma once

#include <QObject>
#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
#include "audiomanager.h"

// Forward declarations
struct AudioApplication;
struct AudioDevice;

// ============================================================================
// ApplicationModel - Individual application sessions
// ============================================================================
class ApplicationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum ApplicationRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ExecutableNameRole,
        IconPathRole,
        VolumeRole,
        IsMutedRole,
        StreamIndexRole,
        IsSystemSoundsRole,
    };
    Q_ENUM(ApplicationRoles)

    explicit ApplicationModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Model management
    void setApplications(const QList<AudioApplication>& applications);
    void updateApplicationVolume(const QString& appId, int volume);
    void updateApplicationMute(const QString& appId, bool muted);
    void updateApplicationAudioLevel(const QString& appId, int audioLevel);

private:
    QList<AudioApplication> m_applications;
    int findApplicationIndex(const QString& appId) const;
};

// ============================================================================
// FilteredDeviceModel - Audio devices (input/output filtered)
// ============================================================================
class FilteredDeviceModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("FilteredDeviceModel is only available through AudioBridge")
    Q_PROPERTY(int currentDefaultIndex READ getCurrentDefaultIndex NOTIFY currentDefaultIndexChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum DeviceRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        DescriptionRole,
        IsDefaultRole,
        IsDefaultCommunicationRole,
        StateRole,
        VendorIdRole,
        ProductIdRole,
        BatteryPercentageRole,
        BatteryStatusRole
    };
    Q_ENUM(DeviceRoles)

    explicit FilteredDeviceModel(bool isInputFilter, QObject *parent = nullptr);

    // QAbstractListModel interface
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Model management
    void setDevices(const QList<AudioDevice>& devices);
    Q_INVOKABLE int getCurrentDefaultIndex() const;
    Q_INVOKABLE QString getDeviceName(int index) const;

signals:
    void currentDefaultIndexChanged();
    void countChanged();

private:
    QList<AudioDevice> m_devices;
    bool m_isInputFilter;
    int m_currentDefaultIndex;
    int findDeviceIndex(const QString& deviceId) const;
    void updateCurrentDefaultIndex();
};

// ============================================================================
// ApplicationGroup - Group of sessions for the same executable
// ============================================================================
struct ApplicationGroup {
    QString executableName;
    QString displayName;
    QString iconPath;
    QList<AudioApplication> sessions;
    int averageVolume;
    bool anyMuted;
    bool allMuted;
    int sessionCount;
    int averageAudioLevel;
    bool isSystemSounds = false;
};

// ============================================================================
// GroupedApplicationModel - Applications grouped by executable
// ============================================================================
class GroupedApplicationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum GroupRoles {
        ExecutableNameRole = Qt::UserRole + 1,
        DisplayNameRole,
        IconPathRole,
        AverageVolumeRole,
        AnyMutedRole,
        AllMutedRole,
        SessionCountRole,
        AverageAudioLevelRole,
        IsSystemSoundsRole
    };
    Q_ENUM(GroupRoles)

    explicit GroupedApplicationModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Model management
    void setGroups(const QList<ApplicationGroup>& groups);
    QList<ApplicationGroup> getGroups() const { return m_groups; }
    void updateGroupVolume(const QString& executableName, int averageVolume);
    void updateGroupMute(const QString& executableName, bool anyMuted, bool allMuted);
    void updateGroupAudioLevel(const QString& executableName, int averageAudioLevel);

private:
    QList<ApplicationGroup> m_groups;
};

// ============================================================================
// ExecutableSessionModel - Sessions for a specific executable
// ============================================================================
class ExecutableSessionModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    enum SessionRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ExecutableNameRole,
        IconPathRole,
        VolumeRole,
        IsMutedRole,
        StreamIndexRole,
        IsSystemSoundsRole
    };
    Q_ENUM(SessionRoles)

    explicit ExecutableSessionModel(QObject *parent = nullptr);

    // QAbstractListModel interface
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void updateSessionVolume(const QString& appId, int volume);
    void updateSessionMute(const QString& appId, bool muted);

    // Model management
    void setSessions(const QList<AudioApplication>& sessions);

private:
    QList<AudioApplication> m_sessions;
};
