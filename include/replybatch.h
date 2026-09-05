#pragma once
#include <QNetworkReply>
#include <QList>
#include <utility>

inline void cancelReplyBatch(QList<QNetworkReply*>& active, QObject* receiver)
{
    const auto replies = std::exchange(active, {});
    for (auto* reply : replies)
    {
        QObject::disconnect(reply, nullptr, receiver, nullptr);
        reply->abort();
        reply->deleteLater();
    }
}
