#include "audiomodels.h"
#include "audiobridge.h"
#include <algorithm>

// ============================================================================
// ApplicationModel Implementation
// ============================================================================

ApplicationModel::ApplicationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ApplicationModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_applications.count();
}

QVariant ApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_applications.count())
        return QVariant();

    const AudioApplication& app = m_applications.at(index.row());

    switch (role) {
    case IdRole:
        return app.id;
    case NameRole:
        return app.name;
    case ExecutableNameRole:
        return app.executableName;
    case IconPathRole:
        return app.iconPath;
    case VolumeRole:
        return app.volume;
    case IsMutedRole:
        return app.isMuted;
    case StreamIndexRole:
        return app.streamIndex;
    case IsSystemSoundsRole:
        return app.isSystemSounds;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ApplicationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "appId";
    roles[NameRole] = "name";
    roles[ExecutableNameRole] = "executableName";
    roles[IconPathRole] = "iconPath";
    roles[VolumeRole] = "volume";
    roles[IsMutedRole] = "isMuted";
    roles[StreamIndexRole] = "streamIndex";
    roles[IsSystemSoundsRole] = "isSystemSounds";
    return roles;
}

void ApplicationModel::setApplications(const QList<AudioApplication>& applications)
{
    beginResetModel();

    QList<AudioApplication> sortedApplications;
    AudioApplication systemSounds;
    bool hasSystemSounds = false;

    for (const AudioApplication& app : applications) {
        if (app.isSystemSounds || app.id == "system_sounds") {
            systemSounds = app;
            hasSystemSounds = true;
        } else {
            sortedApplications.append(app);
        }
    }

    // Sort by name first, then by streamIndex for apps with the same name
    std::sort(sortedApplications.begin(), sortedApplications.end(),
              [](const AudioApplication& a, const AudioApplication& b) {
                  if (a.name.toLower() == b.name.toLower()) {
                      return a.streamIndex < b.streamIndex;
                  }
                  return a.name.toLower() < b.name.toLower();
              });

    if (hasSystemSounds) {
        sortedApplications.append(systemSounds);
    }

    m_applications = sortedApplications;
    endResetModel();
}

void ApplicationModel::updateApplicationVolume(const QString& appId, int volume)
{
    int index = findApplicationIndex(appId);
    if (index >= 0) {
        m_applications[index].volume = volume;
        QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {VolumeRole});
    }
}

void ApplicationModel::updateApplicationMute(const QString& appId, bool muted)
{
    int index = findApplicationIndex(appId);
    if (index >= 0) {
        m_applications[index].isMuted = muted;
        QModelIndex modelIndex = createIndex(index, 0);
        emit dataChanged(modelIndex, modelIndex, {IsMutedRole});
    }
}

void ApplicationModel::updateApplicationAudioLevel(const QString& appId, int audioLevel)
{
    // Audio levels are not stored in the model currently
    // This is for future extension if needed
    Q_UNUSED(appId)
    Q_UNUSED(audioLevel)
}

int ApplicationModel::findApplicationIndex(const QString& appId) const
{
    for (int i = 0; i < m_applications.count(); ++i) {
        if (m_applications[i].id == appId) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// FilteredDeviceModel Implementation
// ============================================================================

FilteredDeviceModel::FilteredDeviceModel(bool isInputFilter, QObject *parent)
    : QAbstractListModel(parent), m_isInputFilter(isInputFilter), m_currentDefaultIndex(-1)
{
}

int FilteredDeviceModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_devices.count();
}

QVariant FilteredDeviceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_devices.count())
        return QVariant();

    const AudioDevice& device = m_devices.at(index.row());

    switch (role) {
    case IdRole:
        return device.id;
    case NameRole:
        return device.name;
    case DescriptionRole:
        return device.description;
    case IsDefaultRole:
        return device.isDefault;
    case IsDefaultCommunicationRole:
        return device.isDefaultCommunication;
    case StateRole:
        return device.state;
    case VendorIdRole:
        return device.vendorId;
    case ProductIdRole:
        return device.productId;
    case BatteryPercentageRole:
        return device.batteryPercentage;
    case BatteryStatusRole:
        return device.batteryStatus;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FilteredDeviceModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "deviceId";
    roles[NameRole] = "name";
    roles[DescriptionRole] = "description";
    roles[IsDefaultRole] = "isDefault";
    roles[IsDefaultCommunicationRole] = "isDefaultCommunication";
    roles[StateRole] = "state";
    roles[VendorIdRole] = "vendorId";
    roles[ProductIdRole] = "productId";
    roles[BatteryPercentageRole] = "batteryPercentage";
    roles[BatteryStatusRole] = "batteryStatus";
    return roles;
}

QString FilteredDeviceModel::getDeviceName(int index) const
{
    if (index >= 0 && index < m_devices.count()) {
        return m_devices[index].name;
    }
    return QString();
}

void FilteredDeviceModel::setDevices(const QList<AudioDevice>& devices)
{
    beginResetModel();

    int oldCount = m_devices.count();
    m_devices.clear();

    for (const AudioDevice& device : devices) {
        if (device.isInput == m_isInputFilter) {
            m_devices.append(device);
        }
    }

    endResetModel();
    updateCurrentDefaultIndex();

    if (oldCount != m_devices.count()) {
        emit countChanged();

        AudioBridge* audioBridge = qobject_cast<AudioBridge*>(parent());
        if (audioBridge) {
            if (m_isInputFilter) {
                emit audioBridge->inputDeviceCountChanged();
            } else {
                emit audioBridge->outputDeviceCountChanged();
            }
        }
    }
}

int FilteredDeviceModel::getCurrentDefaultIndex() const
{
    return m_currentDefaultIndex;
}

void FilteredDeviceModel::updateCurrentDefaultIndex()
{
    int newDefaultIndex = -1;
    for (int i = 0; i < m_devices.count(); ++i) {
        if (m_devices[i].isDefault) {
            newDefaultIndex = i;
            break;
        }
    }

    if (m_currentDefaultIndex != newDefaultIndex) {
        m_currentDefaultIndex = newDefaultIndex;
        emit currentDefaultIndexChanged();
    }
}

int FilteredDeviceModel::findDeviceIndex(const QString& deviceId) const
{
    for (int i = 0; i < m_devices.count(); ++i) {
        if (m_devices[i].id == deviceId) {
            return i;
        }
    }
    return -1;
}

// ============================================================================
// GroupedApplicationModel Implementation
// ============================================================================

GroupedApplicationModel::GroupedApplicationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int GroupedApplicationModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_groups.count();
}

QVariant GroupedApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_groups.count())
        return QVariant();

    const ApplicationGroup& group = m_groups.at(index.row());

    switch (role) {
    case ExecutableNameRole:
        return group.executableName;
    case DisplayNameRole:
        return group.displayName;
    case IconPathRole:
        return group.iconPath;
    case AverageVolumeRole:
        return group.averageVolume;
    case AnyMutedRole:
        return group.anyMuted;
    case AllMutedRole:
        return group.allMuted;
    case SessionCountRole:
        return group.sessionCount;
    case AverageAudioLevelRole:
        return group.averageAudioLevel;
    case IsSystemSoundsRole:
        return group.isSystemSounds;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> GroupedApplicationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[ExecutableNameRole] = "executableName";
    roles[DisplayNameRole] = "displayName";
    roles[IconPathRole] = "iconPath";
    roles[AverageVolumeRole] = "averageVolume";
    roles[AnyMutedRole] = "anyMuted";
    roles[AllMutedRole] = "allMuted";
    roles[SessionCountRole] = "sessionCount";
    roles[AverageAudioLevelRole] = "averageAudioLevel";
    roles[IsSystemSoundsRole] = "isSystemSounds";
    return roles;
}

void GroupedApplicationModel::setGroups(const QList<ApplicationGroup>& groups)
{
    beginResetModel();
    m_groups = groups;
    endResetModel();
}

void GroupedApplicationModel::updateGroupVolume(const QString& executableName, int averageVolume)
{
    for (int i = 0; i < m_groups.count(); ++i) {
        if (m_groups[i].executableName == executableName) {
            m_groups[i].averageVolume = averageVolume;
            QModelIndex modelIndex = createIndex(i, 0);
            emit dataChanged(modelIndex, modelIndex, {AverageVolumeRole});
            break;
        }
    }
}

void GroupedApplicationModel::updateGroupMute(const QString& executableName, bool anyMuted, bool allMuted)
{
    for (int i = 0; i < m_groups.count(); ++i) {
        if (m_groups[i].executableName == executableName) {
            m_groups[i].anyMuted = anyMuted;
            m_groups[i].allMuted = allMuted;
            QModelIndex modelIndex = createIndex(i, 0);
            emit dataChanged(modelIndex, modelIndex, {AnyMutedRole, AllMutedRole});
            break;
        }
    }
}

void GroupedApplicationModel::updateGroupAudioLevel(const QString& executableName, int averageAudioLevel)
{
    for (int i = 0; i < m_groups.count(); ++i) {
        if (m_groups[i].executableName == executableName) {
            if (m_groups[i].averageAudioLevel != averageAudioLevel) {
                m_groups[i].averageAudioLevel = averageAudioLevel;
                QModelIndex modelIndex = createIndex(i, 0);
                emit dataChanged(modelIndex, modelIndex, {AverageAudioLevelRole});
            }
            break;
        }
    }
}

// ============================================================================
// ExecutableSessionModel Implementation
// ============================================================================

ExecutableSessionModel::ExecutableSessionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ExecutableSessionModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_sessions.count();
}

QVariant ExecutableSessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_sessions.count())
        return QVariant();

    const AudioApplication& session = m_sessions.at(index.row());

    switch (role) {
    case IdRole:
        return session.id;
    case NameRole:
        return session.name;
    case ExecutableNameRole:
        return session.executableName;
    case IconPathRole:
        return session.iconPath;
    case VolumeRole:
        return session.volume;
    case IsMutedRole:
        return session.isMuted;
    case StreamIndexRole:
        return session.streamIndex;
    case IsSystemSoundsRole:
        return session.isSystemSounds;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ExecutableSessionModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "appId";
    roles[NameRole] = "name";
    roles[ExecutableNameRole] = "executableName";
    roles[IconPathRole] = "iconPath";
    roles[VolumeRole] = "volume";
    roles[IsMutedRole] = "isMuted";
    roles[StreamIndexRole] = "streamIndex";
    roles[IsSystemSoundsRole] = "isSystemSounds";
    return roles;
}

void ExecutableSessionModel::setSessions(const QList<AudioApplication>& sessions)
{
    beginResetModel();
    m_sessions = sessions;
    endResetModel();
}

void ExecutableSessionModel::updateSessionVolume(const QString& appId, int volume)
{
    for (int i = 0; i < m_sessions.count(); ++i) {
        if (m_sessions[i].id == appId) {
            m_sessions[i].volume = volume;
            QModelIndex modelIndex = createIndex(i, 0);
            emit dataChanged(modelIndex, modelIndex, {VolumeRole});
            break;
        }
    }
}

void ExecutableSessionModel::updateSessionMute(const QString& appId, bool muted)
{
    for (int i = 0; i < m_sessions.count(); ++i) {
        if (m_sessions[i].id == appId) {
            m_sessions[i].isMuted = muted;
            QModelIndex modelIndex = createIndex(i, 0);
            emit dataChanged(modelIndex, modelIndex, {IsMutedRole});
            break;
        }
    }
}
