#include "traymenu.h"

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDataStream>
#include <QEvent>
#include <QImageReader>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QStyleFactory>
#include <QUrl>

namespace {
QIcon menuIcon(const QString& source, const QPalette& palette, int size, qreal dpr)
{
    const QUrl url(source);
    const QString path = url.scheme() == "qrc" ? ":" + url.path() : url.toLocalFile();
    if (path.isEmpty())
        return {};
    QImageReader reader(path);
    reader.setScaledSize(QSize(qRound(size * dpr), qRound(size * dpr)));
    const QImage image = reader.read();
    if (image.isNull())
        return {};
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled, QIcon::Active, QIcon::Selected}) {
        QPixmap pixmap = QPixmap::fromImage(image);
        pixmap.setDevicePixelRatio(dpr);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        const bool selected = mode == QIcon::Active || mode == QIcon::Selected;
        painter.fillRect(pixmap.rect(), palette.color(
            mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active,
            selected ? QPalette::HighlightedText : QPalette::WindowText));
        painter.end();
        icon.addPixmap(pixmap, mode);
    }
    return icon;
}
}

TrayMenu::TrayMenu(QObject* parent)
    : QObject(parent)
    , m_style(QStyleFactory::create("Fusion"))
    , m_menu(std::make_unique<QMenu>())
{
    m_menu->setObjectName("trayContextMenu");
    m_menu->setStyle(m_style.get());
    qApp->installEventFilter(this);
}

TrayMenu::~TrayMenu() = default;

bool TrayMenu::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange)
        applyTheme();
    return QObject::eventFilter(watched, event);
}

void TrayMenu::setColorScheme(Qt::ColorScheme colorScheme)
{
    if (m_colorScheme == colorScheme)
        return;
    m_colorScheme = colorScheme;
    applyTheme();
    emit colorSchemeChanged();
}

void TrayMenu::setEntries(const QVariantList& entries)
{
    if (m_entries == entries)
        return;
    m_entries = entries;
    if (m_menu->isVisible()) {
        // Data-only changes retain the open menu and keyboard selection. If a
        // device changes its capabilities/presets, reopen with the new structure.
        if (!syncMenu(m_menu.get(), m_entries))
            m_menu->close();
        else
            updateIcons(m_menu.get());
    }
    emit entriesChanged();
}

void TrayMenu::popup()
{
    if (m_menu->isVisible())
        return;
    // QMenu::clear() does not own the submenu actions; retire their menus too.
    const auto submenus = m_menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly);
    qDeleteAll(submenus);
    m_menu->clear();
    const QPoint position = QCursor::pos();
    if (auto* screen = QGuiApplication::screenAt(position))
        m_menu->setScreen(screen);
    buildMenu(m_menu.get(), m_entries);
    applyTheme();
    // A QWidget popup works even while all Qt Quick windows are hidden.
    m_menu->popup(position);
    // Activate after showing: Windows relies on this for keyboard focus and
    // dismissing the tray popup when the user clicks outside it.
    m_menu->activateWindow();
}

void TrayMenu::buildMenu(QMenu* menu, const QVariantList& entries)
{
    menu->setStyle(m_style.get());
    menu->setSeparatorsCollapsible(true);
    for (const auto& value : entries) {
        const auto entry = value.toMap();
        QAction* action;
        if (entry.contains("children")) {
            auto* submenu = new QMenu(menu);
            buildMenu(submenu, entry.value("children").toList());
            action = menu->addMenu(submenu);
        } else {
            action = menu->addAction(QString());
            const auto actionId = entry.value("id").toString();
            connect(action, &QAction::triggered, this, [this, actionId] {
                QPointer<TrayMenu> guard(this);
                emit triggered(actionId);
                // The QML handler persists the desired state. Restore checkmarks
                // from that state even when a settings write was rejected.
                if (guard && !syncMenu(m_menu.get(), m_entries))
                    m_menu->close();
            });
        }
        action->setData(entry);
        // Keep icons visible even if the application has disabled menu icons.
        action->setIconVisibleInMenu(true);
    }
    syncMenu(menu, entries);
}

bool TrayMenu::syncMenu(QMenu* menu, const QVariantList& entries)
{
    const auto actions = menu->actions();
    if (actions.size() != entries.size())
        return false;
    for (qsizetype i = 0; i < entries.size(); ++i) {
        auto* action = actions.at(i);
        const auto entry = entries.at(i).toMap();
        if (action->data().toMap().value("id") != entry.value("id")
            || bool(action->menu()) != entry.contains("children"))
            return false;
        action->setData(entry);
        action->setText(entry.value("text").toString());
        action->setSeparator(entry.value("separator", false).toBool());
        action->setVisible(entry.value("visible", true).toBool());
        action->setEnabled(entry.value("enabled", true).toBool());
        action->setCheckable(entry.contains("checked"));
        action->setChecked(entry.value("checked", false).toBool());
        if (action->menu() && !syncMenu(action->menu(), entry.value("children").toList()))
            return false;
    }
    return true;
}

void TrayMenu::applyTheme()
{
    QPalette palette = QGuiApplication::palette();
    if (m_colorScheme != Qt::ColorScheme::Unknown) {
        const bool dark = m_colorScheme == Qt::ColorScheme::Dark;
        const QColor background(dark ? "#242424" : "#f2f2f2");
        const QColor foreground(dark ? "#f2f2f2" : "#1b1b1b");
        for (const auto role : {QPalette::Window, QPalette::Base, QPalette::Button})
            palette.setColor(QPalette::All, role, background);
        for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
            palette.setColor(QPalette::All, role, foreground);
            palette.setColor(QPalette::Disabled, role, dark ? QColor("#9d9d9d") : QColor("#686868"));
        }
    }
    m_menu->setPalette(palette);
    updateIcons(m_menu.get());
}

void TrayMenu::updateIcons(QMenu* menu)
{
    menu->setPalette(m_menu->palette());
    const int size = m_style->pixelMetric(QStyle::PM_SmallIconSize, nullptr, menu);
    for (auto* action : menu->actions()) {
        action->setIcon(cachedIcon(action->data().toMap().value("icon").toString(),
                                   menu->palette(), size, menu->devicePixelRatioF()));
        if (action->menu())
            updateIcons(action->menu());
    }
}

QIcon TrayMenu::cachedIcon(const QString& source, const QPalette& palette, int size, qreal dpr)
{
    if (source.isEmpty())
        return {};
    // Key the colors actually used to tint the icon. Equivalent palettes can
    // have different cacheKey() values after a theme or application-palette event.
    QByteArray key;
    QDataStream stream(&key, QIODevice::WriteOnly);
    stream << source << palette.color(QPalette::Active, QPalette::WindowText).rgba()
           << palette.color(QPalette::Disabled, QPalette::WindowText).rgba()
           << palette.color(QPalette::Active, QPalette::HighlightedText).rgba()
           << size << dpr;
    if (const auto* icon = m_iconCache.object(key))
        return *icon;
    const QIcon icon = menuIcon(source, palette, size, dpr);
    if (!icon.isNull())
        m_iconCache.insert(key, new QIcon(icon));
    return icon;
}
