#pragma once
#include <QJsonDocument>
#include <QString>
namespace JsonStore
{
    QJsonDocument load(const QString& path, const QString& arrayKey = {});
    bool save(const QString& path, const QJsonDocument& document);
    bool validate(const QJsonDocument& document, const QString& arrayKey);
} // namespace JsonStore
