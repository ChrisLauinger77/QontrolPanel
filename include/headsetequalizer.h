#pragma once

#include <QAbstractListModel>
#include <QVariantMap>
#include <QVariantList>
#include <cmath>
#include <optional>

namespace HeadsetEqualizer {
constexpr int MaxBands = 64;

struct Layout {
    int count = 0;
    double baseline = 0;
    double step = 0;
    double minimum = 0;
    double maximum = 0;
    bool operator==(const Layout&) const = default;
    bool valid() const
    {
        return count > 0 && count <= MaxBands && std::isfinite(baseline)
            && std::isfinite(step) && step > 0 && std::isfinite(minimum)
            && std::isfinite(maximum) && minimum < maximum
            && baseline >= minimum && baseline <= maximum;
    }
};

inline bool numeric(const QVariant& value)
{
    switch (value.metaType().id()) {
    case QMetaType::Double: case QMetaType::Float: case QMetaType::Int:
    case QMetaType::UInt: case QMetaType::LongLong: case QMetaType::ULongLong:
        return std::isfinite(value.toDouble());
    default:
        return false;
    }
}

// No padding/truncation: a curve for a different layout must not reach hardware.
inline QVariantList clamp(const QVariantList& bands, const Layout& layout)
{
    if (!layout.valid() || bands.size() != layout.count)
        return {};
    QVariantList result;
    for (const auto& band : bands) {
        if (!numeric(band))
            return {};
        const double bounded = qBound(layout.minimum, band.toDouble(), layout.maximum);
        const double steps = std::round((bounded - layout.minimum) / layout.step);
        result.append(qBound(layout.minimum, layout.minimum + steps * layout.step, layout.maximum));
    }
    return result;
}

// The same bounded schema is used for loading and editing QSettings values.
inline std::optional<QVariantMap> stored(const QVariant& value)
{
    if (value.metaType().id() != QMetaType::QVariantMap)
        return std::nullopt;
    auto entries = value.toMap();
    if (entries.size() > 128)
        return std::nullopt;
    for (auto it = entries.begin(); it != entries.end(); ++it) {
        if (it.key().isEmpty() || it.key().size() > 512
            || it.value().metaType().id() != QMetaType::QVariantMap)
            return std::nullopt;
        const auto entry = it.value().toMap();
        const auto bands = entry.value("bands").toList();
        if (entry.value("custom").metaType().id() != QMetaType::Bool
            || !numeric(entry.value("preset"))
            || entry.value("preset").toDouble() != entry.value("preset").toInt()
            || entry.value("preset").toInt() < 0 || entry.value("preset").toInt() > 255
            || entry.value("bands").metaType().id() != QMetaType::QVariantList
            || bands.size() > MaxBands || (entry.value("custom").toBool() && bands.isEmpty()))
            return std::nullopt;
        for (const auto& band : bands) {
            if (!numeric(band) || std::abs(band.toDouble()) > 100000)
                return std::nullopt;
        }
        it.value() = QVariantMap{{"custom", entry.value("custom").toBool()},
                                {"preset", entry.value("preset").toInt()}, {"bands", bands}};
    }
    return entries;
}
} // namespace HeadsetEqualizer

class HeadsetEqualizerModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit HeadsetEqualizerModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_bands.size(); }
    QVariant data(const QModelIndex& index, int role) const override
    {
        return !index.parent().isValid() && index.column() == 0 && index.row() >= 0
            && index.row() < m_bands.size() && role == Qt::UserRole
            ? m_bands.at(index.row()) : QVariant{};
    }
    QHash<int, QByteArray> roleNames() const override { return {{Qt::UserRole, "bandValue"}}; }
    void setBands(const QVariantList& bands, bool deviceChanged = false)
    {
        if (deviceChanged || m_bands.size() != bands.size()) {
            beginResetModel();
            m_bands = bands;
            endResetModel();
        } else if (m_bands != bands) {
            m_bands = bands;
            if (!bands.isEmpty())
                emit dataChanged(index(0), index(bands.size() - 1), {Qt::UserRole});
        }
    }
private:
    QVariantList m_bands;
};
