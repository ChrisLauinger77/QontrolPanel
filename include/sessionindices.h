#pragma once
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>

// Input order is the initial policy order. Surviving sessions keep their index,
// even when siblings close or the policy's external inputs change.
class SessionIndices
{
public:
    void update(const QList<QPair<QString, QString>>& orderedSessions)
    {
        QMap<QString, QList<QString>> groups;
        for (const auto& session : orderedSessions)
            groups[session.first].append(session.second);
        for (auto it = m_indices.begin(); it != m_indices.end();)
        {
            if (!groups.contains(it.key()))
                it = m_indices.erase(it);
            else
                ++it;
        }
        for (auto group = groups.cbegin(); group != groups.cend(); ++group)
        {
            auto& indices = m_indices[group.key()];
            const QSet<QString> live(group.value().cbegin(), group.value().cend());
            indices.removeIf([&](auto it) { return !live.contains(it.key()); });
            QSet<int> used;
            for (int index : indices)
                used.insert(index);
            for (const auto& id : group.value())
            {
                if (indices.contains(id))
                    continue;
                int index = 0;
                while (used.contains(index))
                    ++index;
                indices[id] = index;
                used.insert(index);
            }
        }
    }
    int index(const QString& executable, const QString& id) const { return m_indices.value(executable).value(id, -1); }

private:
    QMap<QString, QMap<QString, int>> m_indices;
};
