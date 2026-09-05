#include "jsonstore.h"
#include "logmanager.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace
{
    constexpr qint64 maxFileSize = 4 * 1024 * 1024;
    QString inferredKey(const QJsonDocument& document)
    {
        if (document.isObject() && document.object().size() == 1)
            return document.object().keys().first();
        return {};
    }
} // namespace

bool JsonStore::validate(const QJsonDocument& document, const QString& arrayKey)
{
    if (document.isNull())
        return false;
    const QJsonArray entries = arrayKey.isEmpty() ? document.array() : document.object().value(arrayKey).toArray();
    if (arrayKey.isEmpty() ? !document.isArray()
                           : (!document.isObject() || !document.object().value(arrayKey).isArray()))
        return false;
    for (const auto& entry : entries)
    {
        if (arrayKey == "backgroundMutedApps")
        {
            if (!entry.isString() || entry.toString().trimmed().isEmpty())
                return false;
            continue;
        }
        if (!entry.isObject())
            return false;
        const auto object = entry.toObject();
        auto text = [&](const QString& key, bool allowEmpty = false) {
            return object.value(key).isString() && (allowEmpty || !object.value(key).toString().trimmed().isEmpty());
        };
        auto number = [&](const QString& key, int maximum, bool optional = false) {
            const auto value = object.value(key);
            return (optional && value.isUndefined()) ||
                   (value.isDouble() && value.toDouble() >= 0 && value.toDouble() <= maximum &&
                    value.toDouble() == value.toInt());
        };
        if (arrayKey.isEmpty())
        {
            if (!text("executableName") || !number("volumeUpKey", 0x01ffffff) || !number("volumeDownKey", 0x01ffffff) ||
                !number("volumeUpModifiers", 0x7fffffff) || !number("volumeDownModifiers", 0x7fffffff) ||
                !number("volumeStepSize", 100, true))
                return false;
        }
        else if (arrayKey == "commApps")
        {
            if (!text("name") || !text("icon", true))
                return false;
        }
        else
        {
            if (!text("originalName"))
                return false;
            if (arrayKey == "appLocks")
            {
                if (!object.value("isLocked").isBool() || !number("streamIndex", 65535, true))
                    return false;
            }
            else if (arrayKey == "deviceIcons")
            {
                if (!text("iconName"))
                    return false;
            }
            else if (arrayKey == "appRenames" || arrayKey == "executableRenames" || arrayKey == "deviceRenames")
            {
                if (!text("customName", true))
                    return false;
                if (arrayKey == "appRenames" && !number("streamIndex", 65535, true))
                    return false;
            }
            else
                return false;
        }
    }
    return true;
}

QJsonDocument JsonStore::load(const QString& path, const QString& arrayKey)
{
    QFile file(path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly) || file.size() > maxFileSize)
    {
        LOG_WARN("Settings", QString("Cannot read settings file: %1").arg(path));
        return {};
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !validate(document, arrayKey))
    {
        LOG_WARN("Settings", QString("Invalid settings retained for recovery: %1 (%2)").arg(path, error.errorString()));
        return {};
    }
    return document;
}

bool JsonStore::save(const QString& path, const QJsonDocument& document)
{
    const QString key = inferredKey(document);
    if (!validate(document, key))
        return false;
    QFile existing(path);
    if (existing.exists())
    {
        if (!existing.open(QIODevice::ReadOnly))
        {
            LOG_WARN("Settings", QString("Refusing to overwrite unreadable settings: %1").arg(path));
            return false;
        }
        const bool tooLarge = existing.size() > maxFileSize;
        const auto old = tooLarge ? QJsonDocument{} : QJsonDocument::fromJson(existing.readAll());
        existing.close();
        if (!validate(old, key))
        {
            const QString backup = path + ".corrupt-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!QFile::copy(path, backup))
            {
                LOG_WARN("Settings", QString("Cannot preserve corrupt settings; save refused: %1").arg(path));
                return false;
            }
            LOG_WARN("Settings", QString("Preserved invalid settings at: %1").arg(backup));
        }
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const QByteArray data = document.toJson();
    if (data.size() > maxFileSize || !file.open(QIODevice::WriteOnly) || file.write(data) != data.size() ||
        !file.commit())
    {
        LOG_WARN("Settings", QString("Atomic settings save failed: %1 (%2)").arg(path, file.errorString()));
        return false;
    }
    return true;
}
