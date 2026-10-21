#pragma once

#include <QObject>
#include <QCache>
#include <QIcon>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>
#include <memory>

class QMenu;
class QPalette;
class QStyle;

// QML supplies presentation and action IDs; this owns the independent tray popup.
class TrayMenu : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList entries READ entries WRITE setEntries NOTIFY entriesChanged)
    Q_PROPERTY(Qt::ColorScheme colorScheme READ colorScheme WRITE setColorScheme NOTIFY colorSchemeChanged)

public:
    explicit TrayMenu(QObject* parent = nullptr);
    ~TrayMenu() override;

    QVariantList entries() const { return m_entries; }
    void setEntries(const QVariantList& entries);
    Qt::ColorScheme colorScheme() const { return m_colorScheme; }
    void setColorScheme(Qt::ColorScheme colorScheme);
    Q_INVOKABLE void popup();

signals:
    void entriesChanged();
    void colorSchemeChanged();
    void triggered(const QString& actionId);

private:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void buildMenu(QMenu* menu, const QVariantList& entries);
    bool syncMenu(QMenu* menu, const QVariantList& entries);
    void applyTheme();
    void updateIcons(QMenu* menu);
    QIcon cachedIcon(const QString& source, const QPalette& palette, int size, qreal dpr);

    QVariantList m_entries;
    Qt::ColorScheme m_colorScheme = Qt::ColorScheme::Unknown;
    QCache<QByteArray, QIcon> m_iconCache{64};
    std::unique_ptr<QStyle> m_style;
    std::unique_ptr<QMenu> m_menu;
};
