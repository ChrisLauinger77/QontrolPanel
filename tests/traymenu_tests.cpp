#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QFileInfo>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QStyleOptionMenuItem>
#include <cmath>
#include "traymenu.h"

namespace {
constexpr auto Module = "ChrisLauinger77.QontrolPanel";
QMenu* popupMenu()
{
    for (auto* widget : QApplication::topLevelWidgets()) {
        if (widget->objectName() == "trayContextMenu")
            return qobject_cast<QMenu*>(widget);
    }
    return nullptr;
}

QAction* actionFor(QMenu* menu, const QString& id)
{
    for (auto* action : menu->actions()) {
        if (action->data().toMap().value("id").toString() == id)
            return action;
    }
    return nullptr;
}

QColor iconColor(const QIcon& icon, QIcon::Mode mode)
{
    const auto image = icon.pixmap(16, 16, mode).toImage();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (color.alpha() == 255)
                return color;
        }
    }
    return {};
}

qreal contrastRatio(const QColor& foreground, const QColor& background)
{
    const auto luminance = [](const QColor& color) {
        const auto linear = [](qreal channel) {
            return channel <= 0.04045 ? channel / 12.92
                                     : std::pow((channel + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF())
             + 0.0722 * linear(color.blueF());
    };
    const auto first = luminance(foreground);
    const auto second = luminance(background);
    return (qMax(first, second) + 0.05) / (qMin(first, second) + 0.05);
}

QVariantList testEntries()
{
    return {
        QVariantMap{{"id", "play"}, {"text", "Play"}, {"icon", "qrc:/icons/play.png"}},
        QVariantMap{{"id", "headset"}, {"text", "Headset"}, {"icon", "qrc:/icons/headset.svg"},
                    {"children", QVariantList{
                        QVariantMap{{"id", "lights"}, {"text", "Lights"}, {"checked", false},
                                    {"icon", "qrc:/icons/brightness.svg"}}}}}
    };
}
}

class TrayMenuTests : public QObject
{
    Q_OBJECT
    int m_mediaType = -1;
    int m_audioType = -1;
    int m_settingsType = -1;
    int m_headsetType = -1;
    int m_batteryType = -1;
    int m_powerType = -1;

private slots:
    void initTestCase()
    {
        qApp->setQuitOnLastWindowClosed(false);
        qmlRegisterType<TrayMenu>(Module, 1, 0, "TrayMenu");
        const auto stub = QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/tests/qml/TrayUiStubs.qml");
        for (const auto* name : {"Constants", "Utils"})
            qmlRegisterSingletonType(stub, Module, 1, 0, name);
        m_audioType = qmlRegisterSingletonType(stub, Module, 1, 0, "AudioBridge");
        m_mediaType = qmlRegisterSingletonType(stub, Module, 1, 0, "MediaSessionBridge");
        m_settingsType = qmlRegisterSingletonType(stub, Module, 1, 0, "UserSettings");
        m_headsetType = qmlRegisterSingletonType(stub, Module, 1, 0, "HeadsetControlBridge");
        m_powerType = qmlRegisterSingletonType(stub, Module, 1, 0, "PowerBridge");
        m_batteryType = qmlRegisterSingletonType(
            QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/Singletons/HeadsetBattery.qml"),
            Module, 1, 0, "HeadsetBattery");
    }

    void batteryTimeFormatter_data()
    {
        QTest::addColumn<QString>("status");
        QTest::addColumn<int>("toFull");
        QTest::addColumn<int>("toEmpty");
        QTest::addColumn<QString>("expected");
        auto row = [](const char* name, const char* status, int full, int empty, const char* expected) {
            QTest::newRow(name) << QString::fromLatin1(status) << full << empty << QString::fromLatin1(expected);
        };
        row("profile-2", "BATTERY_CHARGING", 60, -1, "about 1 h to full");
        row("profile-0", "BATTERY_AVAILABLE", -1, 302, "about 5 h left");
        row("profile-6", "BATTERY_AVAILABLE", -1, 720, "about 12 h left");
        row("profile-7", "BATTERY_AVAILABLE", -1, 72, "about 1 h left");
        row("zero-charging", "BATTERY_CHARGING", 0, -1, "about 0 min to full");
        row("zero-discharging", "BATTERY_AVAILABLE", -1, 0, "about 0 min left");
        row("minutes-charging", "BATTERY_CHARGING", 45, 300, "about 45 min to full");
        row("minutes-discharging", "BATTERY_AVAILABLE", 60, 45, "about 45 min left");
        row("last-minute", "BATTERY_CHARGING", 59, -1, "about 59 min to full");
        row("round-down", "BATTERY_AVAILABLE", -1, 89, "about 1 h left");
        row("round-up", "BATTERY_AVAILABLE", -1, 90, "about 2 h left");
        row("charging-missing", "BATTERY_CHARGING", -1, 300, "");
        row("discharging-missing", "BATTERY_AVAILABLE", 60, -1, "");
        row("both-missing", "BATTERY_AVAILABLE", -1, -1, "");
        row("negative-estimate", "BATTERY_CHARGING", -30, 300, "");
        row("unavailable", "BATTERY_UNAVAILABLE", 60, 300, "");
        row("hid-error", "BATTERY_HIDERROR", 60, 300, "");
        row("timeout", "BATTERY_TIMEOUT", 60, 300, "");
        row("unknown-status", "unknown", 60, 300, "");
    }

    void batteryTimeFormatter()
    {
        QFETCH(QString, status); QFETCH(int, toFull); QFETCH(int, toEmpty); QFETCH(QString, expected);
        QQmlEngine engine;
        auto* formatter = engine.singletonInstance<QObject*>(m_batteryType);
        QVERIFY(formatter);
        QString actual;
        QVERIFY(QMetaObject::invokeMethod(formatter, "formatTime", Q_RETURN_ARG(QString, actual),
                                         Q_ARG(QString, status), Q_ARG(int, toFull), Q_ARG(int, toEmpty)));
        QCOMPARE(actual, expected);
    }

    void themesRetintIconsAndSubmenus()
    {
        TrayMenu tray;
        tray.setEntries(testEntries());
        tray.setColorScheme(Qt::ColorScheme::Dark);
        tray.popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        QTRY_VERIFY(menu->isVisible());
        QVERIFY(menu->palette().window().color().lightness() < 128);
        auto* play = actionFor(menu, "play");
        auto* headset = actionFor(menu, "headset");
        QVERIFY(play);
        QVERIFY(headset && headset->menu());
        QVERIFY(!headset->icon().isNull()); // SVG assets must load, as well as PNGs.
        QCOMPARE(iconColor(play->icon(), QIcon::Normal), menu->palette().color(QPalette::WindowText));
        QCOMPARE(iconColor(play->icon(), QIcon::Disabled), menu->palette().color(QPalette::Disabled, QPalette::WindowText));
        QCOMPARE(headset->menu()->palette(), menu->palette());
        menu->setActiveAction(play);

        tray.setColorScheme(Qt::ColorScheme::Light);

        QVERIFY(menu->isVisible());
        QCOMPARE(menu->activeAction(), play);
        QVERIFY(menu->palette().window().color().lightness() > 128);
        QCOMPARE(iconColor(play->icon(), QIcon::Normal), menu->palette().color(QPalette::WindowText));
        QCOMPARE(headset->menu()->palette(), menu->palette());
        QCOMPARE(iconColor(headset->icon(), QIcon::Normal), menu->palette().color(QPalette::WindowText));
    }

    void checkedIconsHaveIndependentIndicators_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<bool>("selected");
        QTest::addColumn<bool>("rtl");
        QTest::addColumn<int>("dpr");
        for (bool dark : {false, true})
            for (bool selected : {false, true})
                for (bool rtl : {false, true})
                    for (int dpr : {1, 2})
                        QTest::newRow(qPrintable(QString("dark-%1-hover-%2-rtl-%3-dpr-%4")
                            .arg(dark).arg(selected).arg(rtl).arg(dpr))) << dark << selected << rtl << dpr;
    }

    void checkedIconsHaveIndependentIndicators()
    {
        QFETCH(bool, dark); QFETCH(bool, selected); QFETCH(bool, rtl); QFETCH(int, dpr);
        TrayMenu tray;
        tray.setEntries({QVariantMap{{"id", "device"}, {"text", "Device"},
            {"checked", true}, {"icon", "qrc:/icons/headset.svg"}}});
        tray.setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
        tray.popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        QStyleOptionMenuItem option;
        option.initFrom(menu);
        option.rect = QRect(0, 0, 280, 32);
        option.direction = rtl ? Qt::RightToLeft : Qt::LeftToRight;
        option.state = QStyle::State_Enabled | (selected ? QStyle::State_Selected : QStyle::State_None);
        option.menuHasCheckableItems = true;
        option.checkType = QStyleOptionMenuItem::NonExclusive;
        option.maxIconWidth = 16;
        option.icon = menu->actions().first()->icon();
        option.text = "Device";
        option.font = menu->font();
        option.fontMetrics = QFontMetrics(option.font);
        const auto render = [&](bool checked) {
            QImage image(option.rect.size() * dpr, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(dpr);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            option.checked = checked;
            menu->style()->drawControl(QStyle::CE_MenuItem, &option, &painter, menu);
            return image;
        };
        const auto unchecked = render(false);
        const auto checked = render(true);
        const auto foreground = option.palette.color(selected ? QPalette::HighlightedText : QPalette::WindowText);
        QRect indicatorInk, ordinaryInk;
        for (int y = 0; y < checked.height(); ++y) {
            for (int x = 0; x < checked.width(); ++x) {
                const QRect pixel(x, y, 1, 1);
                if (checked.pixel(x, y) != unchecked.pixel(x, y))
                    indicatorInk |= pixel;
                if (unchecked.pixelColor(x, y) == foreground)
                    ordinaryInk |= pixel;
            }
        }
        QVERIFY(!indicatorInk.isEmpty());
        QVERIFY(!ordinaryInk.isEmpty()); // Keep the device icon and label visible.
        // Checking a device adds a mark entirely beside its icon, including on
        // hover, in either reading direction, and at both display scales.
        if (rtl)
            QVERIFY(indicatorInk.left() > ordinaryInk.right());
        else
            QVERIFY(indicatorInk.right() < ordinaryInk.left());
    }

    void updatesPreserveActionsAndRebuildsRetireSubmenus()
    {
        TrayMenu tray;
        auto entries = testEntries();
        tray.setEntries(entries);
        tray.popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        auto* play = actionFor(menu, "play");
        const auto playIcon = play->icon().cacheKey();
        const auto headsetIcon = actionFor(menu, "headset")->icon().cacheKey();
        QSignalSpy triggered(&tray, &TrayMenu::triggered);
        auto playing = entries[0].toMap();
        playing["text"] = "Pause";
        playing["enabled"] = false;
        entries[0] = playing;
        tray.setEntries(entries);
        QVERIFY(menu->isVisible());
        QCOMPARE(actionFor(menu, "play"), play);
        QCOMPARE(play->icon().cacheKey(), playIcon);
        QCOMPARE(actionFor(menu, "headset")->icon().cacheKey(), headsetIcon);
        QCOMPARE(play->text(), QStringLiteral("Pause"));
        QVERIFY(!play->isEnabled());
        auto* lights = actionFor(actionFor(menu, "headset")->menu(), "lights");
        lights->trigger();
        QCOMPARE(triggered.size(), 1);
        QCOMPARE(triggered[0][0].toString(), QStringLiteral("lights"));
        QVERIFY(!lights->isChecked()); // No accepted setting change was published.

        QPointer<QMenu> oldSubmenu = actionFor(menu, "headset")->menu();
        entries.append(QVariantMap{{"id", "settings"}, {"text", "Settings"}});
        tray.setEntries(entries);
        QVERIFY(!menu->isVisible());
        tray.popup();
        QVERIFY(oldSubmenu.isNull());
        QCOMPARE(menu->actions().size(), 3);
        QCOMPARE(menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly).size(), 1);
        menu->close();
        tray.popup();
        QCOMPARE(menu->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly).size(), 1);
    }

    void forcedThemePreservesSelectionColors_data()
    {
        QTest::addColumn<bool>("darkSystemPalette");
        QTest::addColumn<QColor>("highlight");
        QTest::addColumn<QColor>("highlightedText");
        QTest::newRow("light-menu-dark-system-blue-accent")
            << true << QColor("#0078d4") << QColor(Qt::white);
        QTest::newRow("dark-menu-light-system-blue-accent")
            << false << QColor("#0078d4") << QColor(Qt::white);
        QTest::newRow("light-menu-dark-system-dark-accent")
            << true << QColor("#00316f") << QColor(Qt::white);
        QTest::newRow("dark-menu-light-system-light-accent")
            << false << QColor("#a6d8ff") << QColor(Qt::black);
    }

    void forcedThemePreservesSelectionColors()
    {
        QFETCH(bool, darkSystemPalette);
        QFETCH(QColor, highlight);
        QFETCH(QColor, highlightedText);
        const auto original = QGuiApplication::palette();
        const auto restore = qScopeGuard([&] { QApplication::setPalette(original); });
        QPalette systemPalette = original;
        systemPalette.setColor(QPalette::All, QPalette::Window,
                               darkSystemPalette ? QColor("#161616") : QColor("#f8f8f8"));
        systemPalette.setColor(QPalette::All, QPalette::WindowText,
                               darkSystemPalette ? Qt::white : Qt::black);
        systemPalette.setColor(QPalette::All, QPalette::Highlight, highlight);
        systemPalette.setColor(QPalette::All, QPalette::HighlightedText, highlightedText);
        QApplication::setPalette(systemPalette);
        TrayMenu tray;
        tray.setEntries(testEntries());
        tray.setColorScheme(darkSystemPalette ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
        tray.popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        QCOMPARE(menu->palette().window().color().lightness() > 128, darkSystemPalette);
        QCOMPARE(menu->palette().highlight(), systemPalette.highlight());
        QCOMPARE(menu->palette().highlightedText(), systemPalette.highlightedText());
        const auto contrast = contrastRatio(menu->palette().color(QPalette::HighlightedText),
                                            menu->palette().color(QPalette::Highlight));
        QVERIFY2(contrast >= 4.5, qPrintable(QString("Selected text contrast is %1:1").arg(contrast)));
        auto* play = actionFor(menu, "play");
        QVERIFY(play);
        menu->setActiveAction(play);
        for (const auto mode : {QIcon::Active, QIcon::Selected})
            QCOMPARE(iconColor(play->icon(), mode), systemPalette.color(QPalette::HighlightedText));
        QCOMPARE(iconColor(play->icon(), QIcon::Normal), menu->palette().color(QPalette::WindowText));
        auto* submenu = actionFor(menu, "headset")->menu();
        QCOMPARE(submenu->palette(), menu->palette());
    }

    void autoRestoresAndTracksApplicationPalette()
    {
        const auto original = QGuiApplication::palette();
        const auto restore = qScopeGuard([&] { QApplication::setPalette(original); });
        TrayMenu tray;
        tray.setEntries(testEntries());
        tray.setColorScheme(Qt::ColorScheme::Dark);
        tray.popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        QSignalSpy changed(&tray, &TrayMenu::colorSchemeChanged);

        tray.setColorScheme(Qt::ColorScheme::Unknown);

        QCOMPARE(changed.size(), 1);
        QCOMPARE(menu->palette().window(), original.window());
        QCOMPARE(menu->palette().windowText(), original.windowText());
        auto* play = actionFor(menu, "play");
        QCOMPARE(iconColor(play->icon(), QIcon::Normal), original.color(QPalette::WindowText));
        QPalette updated = original;
        updated.setColor(QPalette::All, QPalette::Window, QColor("#313331"));
        updated.setColor(QPalette::All, QPalette::WindowText, QColor("#deedee"));
        QApplication::setPalette(updated);
        QTRY_COMPARE(menu->palette().window(), updated.window());
        QCOMPARE(iconColor(play->icon(), QIcon::Normal), updated.color(QPalette::WindowText));
        tray.setColorScheme(Qt::ColorScheme::Unknown);
        QCOMPARE(changed.size(), 1);
    }

    void writeOnlyHeadsetToggle_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QByteArray>("capability");
        QTest::addColumn<QByteArray>("setting");
        QTest::newRow("volumeLimiter") << QString("volumeLimiter") << QByteArray("hasVolumeLimiterCapability")
            << QByteArray("headsetcontrolVolumeLimiter");
        QTest::newRow("bluetoothWhenPoweredOn") << QString("bluetoothWhenPoweredOn") << QByteArray("hasBluetoothWhenPoweredOnCapability")
            << QByteArray("headsetcontrolBluetoothWhenPoweredOn");
    }

    void writeOnlyHeadsetToggle()
    {
        QFETCH(QString, id); QFETCH(QByteArray, capability); QFETCH(QByteArray, setting);
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* headset = engine.singletonInstance<QObject*>(m_headsetType);
        auto* settings = engine.singletonInstance<QObject*>(m_settingsType);
        headset->setProperty("batteryStatus", "BATTERY_UNAVAILABLE");
        for (const auto* flag : {"hasLightsCapability", "hasVoicePromptsCapability", "hasRotateToMuteCapability",
                                 "hasEqualizerPresetsCapability", "hasAncCapability"})
            QVERIFY(headset->setProperty(flag, false));
        // Disable other new toggles too, leaving just the capability under test.
        QVERIFY(headset->setProperty("hasVolumeLimiterCapability", false));
        QVERIFY(headset->setProperty("hasBluetoothWhenPoweredOnCapability", false));
        QVERIFY(headset->setProperty(capability.constData(), true));
        tray->popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        auto* headsetAction = actionFor(menu, "headset");
        QVERIFY(headsetAction && headsetAction->isVisible());
        auto* toggle = actionFor(headsetAction->menu(), id);
        QVERIFY(toggle && toggle->isVisible() && toggle->isCheckable());
        QCOMPARE(settings->property(setting.constData()).toInt(), -1);
        QVERIFY(!toggle->isChecked());
        toggle->trigger(); // The first explicit choice enables it, even from -1.
        QCOMPARE(settings->property(setting.constData()).toInt(), 1);
        QVERIFY(toggle->isChecked());
        toggle->trigger();
        QCOMPARE(settings->property(setting.constData()).toInt(), 0);
        QVERIFY(!toggle->isChecked());
        QVERIFY(headset->setProperty(capability.constData(), false));
        QVERIFY(!toggle->isVisible());
        QVERIFY(!headsetAction->isVisible());
    }

    void deviceListsFollowSnapshots_data()
    {
        QTest::addColumn<bool>("isInput");
        QTest::newRow("outputs") << false;
        QTest::newRow("inputs") << true;
    }

    void deviceListsFollowSnapshots()
    {
        QFETCH(bool, isInput);
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* audio = engine.singletonInstance<QObject*>(m_audioType);
        auto* outputs = audio->property("outputDevices").value<QObject*>();
        auto* inputs = audio->property("inputDevices").value<QObject*>();
        QVERIFY(outputs && inputs);
        const auto snapshot = [](const QString& prefix, bool secondDefault = false) -> QVariantList {
            return {QVariantMap{{"deviceId", prefix + ":one"}, {"name", "First"}, {"isDefault", !secondDefault}},
                    QVariantMap{{"deviceId", prefix + ":two"}, {"name", "Second"}, {"isDefault", secondDefault}}};
        };
        QVERIFY(outputs->setProperty("devices", snapshot("output")));
        QVERIFY(inputs->setProperty("devices", snapshot("input")));
        const QString renamed(60, 'R');
        QVERIFY(audio->setProperty("deviceRenames", QVariantMap{{"First", renamed}}));
        tray->popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        QTRY_VERIFY(menu->isVisible());
        const auto actions = menu->actions();
        QCOMPARE(actions[0]->text(), QString("Output devices"));
        QVERIFY(!actions[0]->isEnabled());
        QVERIFY(actions[3]->isSeparator());
        QCOMPARE(actions[4]->text(), QString("Input devices"));
        QVERIFY(!actions[4]->isEnabled());
        QVERIFY(actions[7]->isSeparator());
        for (const auto& prefix : {QString("outputDevice:output"), QString("inputDevice:input")}) {
            auto* first = actionFor(menu, prefix + ":one");
            auto* second = actionFor(menu, prefix + ":two");
            QVERIFY(first && second);
            QVERIFY(!first->menu() && !second->menu()); // Two clicks, no submenu.
            QVERIFY(first->isEnabled() && second->isEnabled());
            QVERIFY(first->isCheckable() && second->isCheckable());
            QVERIFY(first->isChecked() && !second->isChecked());
            QCOMPARE(first->font().weight(), QFont::DemiBold);
            QCOMPARE(second->font().weight(), menu->font().weight());
            QCOMPARE(first->text(), renamed.left(47) + "...");
            const auto icon = prefix.startsWith("input") ? "qrc:/icons/mic.svg" : "qrc:/icons/headset.svg";
            QCOMPARE(first->data().toMap().value("icon").toString(), QString(icon));
            QVERIFY(!first->icon().isNull());
        }

        const auto prefix = isInput ? QString("inputDevice:input") : QString("outputDevice:output");
        auto* first = actionFor(menu, prefix + ":one");
        auto* second = actionFor(menu, prefix + ":two");
        menu->setActiveAction(second);
        second->trigger();
        QCOMPARE(audio->property(isInput ? "lastInputDeviceId" : "lastOutputDeviceId").toString(),
                 isInput ? QString("input:two") : QString("output:two"));
        QVERIFY(audio->property(isInput ? "lastOutputDeviceId" : "lastInputDeviceId").toString().isEmpty());
        QVERIFY(first->isChecked() && !second->isChecked()); // Await the actual device update.
        QCOMPARE(first->font().weight(), QFont::DemiBold);
        QCOMPARE(second->font().weight(), menu->font().weight());

        auto* model = isInput ? inputs : outputs;
        auto devices = snapshot(isInput ? "input" : "output", true);
        QVERIFY(model->setProperty("devices", devices));
        QTRY_VERIFY(second->isChecked());
        QVERIFY(!first->isChecked());
        QCOMPARE(first->font().weight(), menu->font().weight());
        QCOMPARE(second->font().weight(), QFont::DemiBold);
        QVERIFY(menu->isVisible());
        QCOMPARE(menu->actions(), actions);
        QCOMPARE(menu->activeAction(), second);

        auto changed = devices[1].toMap();
        changed["name"] = "Renamed device";
        devices[1] = changed;
        QVERIFY(model->setProperty("devices", devices));
        QTRY_COMPARE(second->text(), QString("Renamed device"));
        QVERIFY(audio->setProperty("deviceRenames", QVariantMap{{"Renamed device", "Custom name"}}));
        QTRY_COMPARE(second->text(), QString("Custom name"));
        QVERIFY(menu->isVisible());
        QCOMPARE(menu->actions(), actions);

        // A disappearing row may close the menu; the action keeps its device ID.
        devices.removeFirst();
        QVERIFY(model->setProperty("devices", devices));
        QTRY_VERIFY(!menu->isVisible());
        second->trigger();
        QCOMPARE(audio->property(isInput ? "lastInputDeviceId" : "lastOutputDeviceId").toString(),
                 isInput ? QString("input:two") : QString("output:two"));
    }

    void deviceSectionsFallBackIndependently_data()
    {
        QTest::addColumn<int>("outputCount");
        QTest::addColumn<int>("inputCount");
        QTest::addColumn<bool>("ready");
        QTest::newRow("two-outputs-one-input") << 2 << 1 << true;
        QTest::newRow("one-output-two-inputs") << 1 << 2 << true;
        QTest::newRow("no-devices") << 0 << 0 << true;
        QTest::newRow("loading") << 2 << 2 << false;
    }

    void deviceSectionsFallBackIndependently()
    {
        QFETCH(int, outputCount); QFETCH(int, inputCount); QFETCH(bool, ready);
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* audio = engine.singletonInstance<QObject*>(m_audioType);
        QVERIFY(audio->setProperty("isReady", ready));
        for (bool isInput : {false, true}) {
            auto* model = audio->property(isInput ? "inputDevices" : "outputDevices").value<QObject*>();
            QVERIFY(model);
            QVariantList devices;
            for (int i = 0; i < (isInput ? inputCount : outputCount); ++i)
                devices.append(QVariantMap{{"deviceId", QString::number(i)}, {"name", "Test device"}, {"isDefault", i == 0}});
            QVERIFY(model->setProperty("devices", devices));
        }
        tray->popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        const auto actions = menu->actions();
        int row = 0;
        for (bool isInput : {false, true}) {
            const auto prefix = isInput ? QString("inputDevice:") : QString("outputDevice:");
            const int count = isInput ? inputCount : outputCount;
            auto* header = actions[row++];
            QVERIFY(!header->isEnabled());
            QVERIFY(!header->isCheckable());
            if (ready && count >= 2) {
                QCOMPARE(header->text(), isInput ? QString("Input devices") : QString("Output devices"));
                for (int i = 0; i < count; ++i)
                    QCOMPARE(actions[row++], actionFor(menu, prefix + QString::number(i)));
                if (!isInput)
                    QVERIFY(actions[row++]->isSeparator());
            } else {
                const auto name = !ready ? QString("Loading...")
                    : isInput ? QString("Test microphone") : QString("Test speakers");
                QCOMPARE(header->text(), (isInput ? "Input: " : "Output: ") + name);
                QCOMPARE(header->data().toMap().value("icon").toString(),
                         isInput ? QString("qrc:/icons/mic.svg") : QString("qrc:/icons/panel_volume_100.svg"));
                for (auto* action : actions)
                    QVERIFY(!action->data().toMap().value("id").toString().startsWith(prefix));
            }
        }
        QVERIFY(actions[row]->isSeparator());
        QCOMPARE(actions[row + 1], actionFor(menu, "previous"));
    }

    void ancSubmenuShowsSavedChoice()
    {
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* headset = engine.singletonInstance<QObject*>(m_headsetType);
        auto* settings = engine.singletonInstance<QObject*>(m_settingsType);
        QVERIFY(headset->setProperty("batteryStatus", "BATTERY_UNAVAILABLE"));
        for (const auto* flag : {"hasLightsCapability", "hasVoicePromptsCapability", "hasRotateToMuteCapability",
                                 "hasEqualizerPresetsCapability", "hasVolumeLimiterCapability",
                                 "hasBluetoothWhenPoweredOnCapability"})
            QVERIFY(headset->setProperty(flag, false));
        tray->popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        auto* headsetAction = actionFor(menu, "headset");
        QVERIFY(headsetAction && headsetAction->isVisible()); // ANC alone keeps HeadsetControl visible.
        auto* anc = actionFor(headsetAction->menu(), "anc");
        QVERIFY(anc && anc->isVisible() && anc->menu());
        QCOMPARE(anc->text(), QString("Noise cancelling"));
        QCOMPARE(anc->menu()->actions().size(), 3);
        QCOMPARE(settings->property("headsetcontrolAnc").toInt(), -1);
        for (auto* action : anc->menu()->actions())
            QVERIFY(action->isCheckable() && !action->isChecked());
        for (int mode = 0; mode <= 2; ++mode) {
            auto* action = actionFor(anc->menu(), QString("anc:%1").arg(mode));
            QVERIFY(action);
            action->trigger();
            QCOMPARE(settings->property("headsetcontrolAnc").toInt(), mode);
            for (int other = 0; other <= 2; ++other)
                QCOMPARE(actionFor(anc->menu(), QString("anc:%1").arg(other))->isChecked(), other == mode);
        }
        QVERIFY(settings->setProperty("headsetcontrolAnc", -1));
        for (auto* action : anc->menu()->actions())
            QVERIFY(!action->isChecked());
        QVERIFY(headset->setProperty("hasAncCapability", false));
        QVERIFY(!anc->isVisible());
        QVERIFY(!headsetAction->isVisible());
        QVERIFY(headset->setProperty("hasAncCapability", true));
        QVERIFY(anc->isVisible());
        QVERIFY(headsetAction->isVisible());
        QVERIFY(headset->setProperty("anyDeviceFound", false));
        QVERIFY(!headsetAction->isVisible());
    }

    void powerSubmenuFollowsSettingAndCapabilities()
    {
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* power = engine.singletonInstance<QObject*>(m_powerType);
        auto* settings = engine.singletonInstance<QObject*>(m_settingsType);
        tray->popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        auto* powerAction = actionFor(menu, "power");
        QVERIFY(powerAction && powerAction->isVisible() && powerAction->isEnabled() && powerAction->menu());
        auto* powerMenu = powerAction->menu();
        QCOMPARE(powerMenu->actions().size(), 9);
        for (auto* action : powerMenu->actions()) {
            if (!action->isSeparator())
                QVERIFY2(!action->icon().isNull(), qPrintable(action->text()));
        }
        // The stub supports sleep and UEFI, but not hibernate or switching users.
        for (const auto* id : {"sleep", "restart", "restartUefi", "shutdown", "lock", "signOut"})
            QVERIFY2(actionFor(powerMenu, id)->isEnabled(), id);
        QVERIFY(!actionFor(powerMenu, "hibernate")->isEnabled());
        QVERIFY(!actionFor(powerMenu, "switchUser")->isEnabled());
        QVERIFY(power->setProperty("hibernateSupported", true));
        QVERIFY(actionFor(powerMenu, "hibernate")->isEnabled());
        QVERIFY(power->setProperty("multipleUsers", true));
        QVERIFY(actionFor(powerMenu, "switchUser")->isEnabled());
        QVERIFY(power->setProperty("sleepSupported", false));
        QVERIFY(!actionFor(powerMenu, "sleep")->isEnabled());
        QVERIFY(power->setProperty("uefiSupported", false));
        QVERIFY(!actionFor(powerMenu, "restartUefi")->isEnabled());
        // A running power action disables the whole submenu, as in the panel.
        QVERIFY(power->setProperty("busy", true));
        QVERIFY(!powerAction->isEnabled());
        QVERIFY(power->setProperty("busy", false));
        QVERIFY(powerAction->isEnabled());
        QVERIFY(settings->setProperty("enablePowerMenu", false));
        QVERIFY(!powerAction->isVisible());
        QVERIFY(settings->setProperty("enablePowerMenu", true));
        QVERIFY(powerAction->isVisible());
    }

    void powerSubmenuAction_data()
    {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("bridgeCall");
        QTest::addColumn<int>("confirmAction");
        // Confirmed actions use the ConfirmPowerActionDialog indexes; -1 runs immediately.
        QTest::newRow("sleep") << QString("sleep") << QString("sleep") << -1;
        QTest::newRow("hibernate") << QString("hibernate") << QString("hibernate") << 0;
        QTest::newRow("restart") << QString("restart") << QString("restart") << 1;
        QTest::newRow("shutdown") << QString("shutdown") << QString("shutdown") << 2;
        QTest::newRow("signOut") << QString("signOut") << QString("signOut") << 3;
        QTest::newRow("restartUefi") << QString("restartUefi") << QString("uefi") << 4;
        QTest::newRow("lock") << QString("lock") << QString("lock") << -1;
        QTest::newRow("switchUser") << QString("switchUser") << QString("switchAccount") << -1;
    }

    void powerSubmenuAction()
    {
        QFETCH(QString, id); QFETCH(QString, bridgeCall); QFETCH(int, confirmAction);
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* power = engine.singletonInstance<QObject*>(m_powerType);
        auto* settings = engine.singletonInstance<QObject*>(m_settingsType);
        QVERIFY(power->setProperty("hibernateSupported", true));
        QVERIFY(power->setProperty("multipleUsers", true));
        tray->popup();
        auto* menu = popupMenu();
        QVERIFY(menu);
        auto* powerAction = actionFor(menu, "power");
        QVERIFY(powerAction && powerAction->menu());
        auto* action = actionFor(powerAction->menu(), id);
        QVERIFY(action && action->isEnabled());
        QSignalSpy confirmation(systemTray.get(), SIGNAL(powerConfirmationRequested(int)));
        action->trigger();
        QCOMPARE(power->property("lastPowerAction").toString(), bridgeCall);
        QCOMPARE(confirmation.size(), 0);

        // With confirmation enabled, the tray hands these actions to the countdown window.
        QVERIFY(power->setProperty("lastPowerAction", QString()));
        QVERIFY(settings->setProperty("showPowerDialogConfirmation", true));
        action->trigger();
        if (confirmAction < 0) {
            QCOMPARE(power->property("lastPowerAction").toString(), bridgeCall);
            QCOMPARE(confirmation.size(), 0);
        } else {
            QVERIFY(power->property("lastPowerAction").toString().isEmpty());
            QCOMPARE(confirmation.size(), 1);
            QCOMPARE(confirmation.at(0).at(0).toInt(), confirmAction);
        }
    }

    void microphoneTooltipShowsLastReading_data()
    {
        QTest::addColumn<int>("micMuted");
        QTest::addColumn<bool>("chatMix");
        QTest::addColumn<QString>("expected");
        for (bool chatMix : {false, true}) {
            const auto suffix = chatMix ? "-chatmix" : "-no-chatmix";
            QTest::newRow(qPrintable(QString("unknown") + suffix)) << -1 << chatMix << QString();
            QTest::newRow(qPrintable(QString("unmuted") + suffix)) << 0 << chatMix << QString("Mic unmuted (last reading)");
            QTest::newRow(qPrintable(QString("muted") + suffix)) << 1 << chatMix << QString("Mic muted (last reading)");
        }
    }

    void microphoneTooltipShowsLastReading()
    {
        QFETCH(int, micMuted); QFETCH(bool, chatMix); QFETCH(QString, expected);
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* headset = engine.singletonInstance<QObject*>(m_headsetType);
        QVERIFY(headset->setProperty("hasChatMixCapability", chatMix));
        QVERIFY(headset->setProperty("micMuted", micMuted));
        const auto tooltip = systemTray->property("tooltip").toString();
        QCOMPARE(tooltip.contains("ChatMix"), chatMix);
        if (micMuted < 0) {
            QVERIFY(!tooltip.contains("Mic muted"));
            QVERIFY(!tooltip.contains("Mic unmuted"));
        } else {
            QVERIFY2(tooltip.contains(expected), qPrintable(tooltip));
            if (chatMix)
                QVERIFY(tooltip.indexOf("ChatMix") < tooltip.indexOf(expected));
        }
        QVERIFY(headset->setProperty("micMuted", -1));
        QVERIFY(!systemTray->property("tooltip").toString().contains("last reading"));
        QVERIFY(headset->setProperty("micMuted", 1));
        QVERIFY(systemTray->property("tooltip").toString().contains("Mic muted (last reading)"));
        QVERIFY(headset->setProperty("anyDeviceFound", false));
        QCOMPARE(systemTray->property("tooltip").toString(), QString("QontrolPanel"));
    }

    void mediaTooltip()
    {
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* headset = engine.singletonInstance<QObject*>(m_headsetType);
        auto* media = engine.singletonInstance<QObject*>(m_mediaType);
        auto* settings = engine.singletonInstance<QObject*>(m_settingsType);
        const auto tooltip = [&] { return systemTray->property("tooltip").toString(); };
        const auto mediaLine = QStringLiteral("\n♫ Test song");
        QVERIFY(!tooltip().contains(QChar(0x266B)));
        QVERIFY(media->setProperty("mediaTitle", " Test song "));
        // Paused media is left out of the tooltip.
        QVERIFY(!tooltip().contains(QChar(0x266B)));
        QVERIFY(media->setProperty("isMediaPlaying", true));
        QVERIFY2(tooltip().endsWith(mediaLine), qPrintable(tooltip()));
        QVERIFY(media->setProperty("mediaArtist", " Test artist "));
        QVERIFY2(tooltip().endsWith(mediaLine + QStringLiteral(" — Test artist")), qPrintable(tooltip()));
        QVERIFY(media->setProperty("isMediaPlaying", false));
        QVERIFY(!tooltip().contains(QChar(0x266B)));
        QVERIFY(media->setProperty("isMediaPlaying", true));
        QVERIFY(media->setProperty("mediaArtist", ""));
        QVERIFY2(tooltip().endsWith(mediaLine), qPrintable(tooltip()));
        QVERIFY(tooltip().contains(QStringLiteral("HeadsetControl Test device")));
        QVERIFY(settings->setProperty("enableMediaSessionManager", false));
        QVERIFY(!tooltip().contains(QChar(0x266B)));
        QVERIFY(settings->setProperty("enableMediaSessionManager", true));
        QVERIFY(headset->setProperty("anyDeviceFound", false));
        QCOMPARE(tooltip(), QStringLiteral("QontrolPanel") + mediaLine);

        // Long titles are shortened so the headset status before them stays within the Windows limit.
        QVERIFY(headset->setProperty("anyDeviceFound", true));
        QVERIFY(headset->setProperty("micMuted", 0));
        QVERIFY(headset->setProperty("batteryIcon", QStringLiteral("\U0001F50B")));
        const auto headsetTooltip = QStringLiteral("QontrolPanel\n\U0001F3A7 HeadsetControl Test device\n\U0001F50B 50% "
                                                   "\U0001F3A4ChatMix: 50 Mic unmuted (last reading)");
        QVERIFY(media->setProperty("mediaTitle", QString(200, QChar('x'))));
        QVERIFY2(tooltip().startsWith(headsetTooltip + QStringLiteral("\n♫ x")), qPrintable(tooltip()));
        QVERIFY(tooltip().endsWith(QStringLiteral("x...")));
        QCOMPARE(tooltip().size(), 127);
        QVERIFY(media->setProperty("mediaTitle", QStringLiteral("Test song")));
        QVERIFY(media->setProperty("mediaArtist", QString(200, QChar('y'))));
        QVERIFY2(tooltip().startsWith(headsetTooltip + mediaLine + QStringLiteral(" — y")), qPrintable(tooltip()));
        QVERIFY(tooltip().endsWith(QStringLiteral("y...")));
        QCOMPARE(tooltip().size(), 127);
        QVERIFY(media->setProperty("mediaArtist", ""));
        // Without room for a readable title, the media line is left out.
        QVERIFY(headset->setProperty("displayName", QString(60, QChar('h'))));
        QVERIFY(!tooltip().contains(QChar(0x266B)));
        QVERIFY(media->setProperty("mediaTitle", ""));
        QVERIFY(headset->setProperty("displayName", QStringLiteral("HeadsetControl Test device")));
        QCOMPARE(tooltip(), headsetTooltip);
    }

    void actualTrayBindingsAndActions()
    {
        QQmlEngine engine;
        auto paths = engine.importPathList();
        paths.removeIf([](const QString& path) {
            const auto directory = path.startsWith("qrc:/") ? path.mid(3) : path;
            return QFileInfo::exists(directory + "/ChrisLauinger77/QontrolPanel/qmldir");
        });
        engine.setImportPathList(paths);
        QQmlComponent component(&engine, QUrl::fromLocalFile(QONTROLPANEL_SOURCE_DIR "/qml/SystemTray.qml"));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> systemTray(component.createWithInitialProperties({{"visible", false}}));
        QVERIFY2(systemTray != nullptr, qPrintable(component.errorString()));
        auto* tray = qobject_cast<TrayMenu*>(systemTray->property("contextMenu").value<QObject*>());
        QVERIFY(tray);
        auto* headset = engine.singletonInstance<QObject*>(m_headsetType);
        QVERIFY(systemTray->property("tooltip").toString().contains(QStringLiteral("HeadsetControl Test device")));
        QVERIFY(headset->setProperty("batteryStatus", "BATTERY_CHARGING"));
        QVERIFY(headset->setProperty("timeToFullMinutes", 60));
        QTRY_VERIFY(systemTray->property("tooltip").toString().contains("50% (about 1 h to full)"));
        QVERIFY(headset->setProperty("timeToEmptyMinutes", 302));
        QVERIFY(headset->setProperty("batteryStatus", "BATTERY_AVAILABLE"));
        QTRY_VERIFY(systemTray->property("tooltip").toString().contains("50% (about 5 h left)"));
        QVERIFY(headset->setProperty("timeToEmptyMinutes", -1));
        QTRY_VERIFY(!systemTray->property("tooltip").toString().contains("about "));
        QVERIFY(headset->setProperty("timeToFullMinutes", -1));
        // Charging headsets may report no level yet; never show it as -1%.
        QVERIFY(headset->setProperty("batteryLevel", -1));
        QVERIFY(headset->setProperty("batteryStatus", "BATTERY_CHARGING"));
        QVERIFY(headset->setProperty("batteryIcon", QStringLiteral("\u26A1\uFE0E")));
        QTRY_VERIFY(systemTray->property("tooltip").toString().contains(QStringLiteral("\n\u26A1\uFE0E Charging")));
        QVERIFY(!systemTray->property("tooltip").toString().contains("-1%"));
        QVERIFY(headset->setProperty("batteryLevel", 50));
        QVERIFY(headset->setProperty("batteryStatus", "BATTERY_AVAILABLE"));
        // A newly paired headset changes the display name without changing the driver name.
        headset->setProperty("displayName", QStringLiteral("Jabra Evolve2 85"));
        QTRY_VERIFY(systemTray->property("tooltip").toString().contains(QStringLiteral("Jabra Evolve2 85")));
        QCOMPARE(headset->property("deviceName").toString(), QStringLiteral("Test headset"));
        // Exercise the actual right-click handler, with no Qt Quick window.
        QQmlExpression contextRequest(qmlContext(systemTray.get()), systemTray.get(),
                                     "activated(Platform.SystemTrayIcon.Context)");
        contextRequest.evaluate();
        QVERIFY2(!contextRequest.hasError(), qPrintable(contextRequest.error().toString()));
        auto* menu = popupMenu();
        QVERIFY(menu);
        QTRY_VERIFY(menu->isVisible());
        for (auto* action : menu->actions()) {
            if (!action->isSeparator())
                QVERIFY2(!action->icon().isNull(), qPrintable(action->text()));
        }
        auto* previous = actionFor(menu, "previous");
        QVERIFY(previous && !previous->isEnabled());
        auto* media = engine.singletonInstance<QObject*>(m_mediaType);
        media->setProperty("canPreviousTrack", true);
        QVERIFY(previous->isEnabled());
        previous->trigger();
        QCOMPARE(media->property("previousTrackCount").toInt(), 1);
        actionFor(menu, "playPause")->trigger();
        QCOMPARE(media->property("playPauseCount").toInt(), 1);
        QCOMPARE(actionFor(menu, "playPause")->text(), QStringLiteral("Pause"));

        auto* settings = engine.singletonInstance<QObject*>(m_settingsType);
        auto* headsetMenu = actionFor(menu, "headset")->menu();
        auto* lights = actionFor(headsetMenu, "lights");
        QVERIFY(!lights->isChecked());
        lights->trigger();
        QVERIFY(settings->property("headsetcontrolLights").toBool());
        QVERIFY(lights->isChecked());
        actionFor(actionFor(headsetMenu, "equalizer")->menu(), "equalizer:1")->trigger();
        QCOMPARE(headset->property("equalizerPreset").toInt(), 1);
        auto* equalizerMenu = actionFor(headsetMenu, "equalizer")->menu();
        QVERIFY(actionFor(equalizerMenu, "equalizer:1")->isChecked());
        headset->setProperty("customEqualizerEnabled", true);
        QVERIFY(!actionFor(equalizerMenu, "equalizer:1")->isChecked());
        actionFor(equalizerMenu, "equalizer:0")->trigger();
        QVERIFY(!headset->property("customEqualizerEnabled").toBool());
        QVERIFY(actionFor(equalizerMenu, "equalizer:0")->isChecked());

        settings->setProperty("enableMediaSessionManager", false);
        QVERIFY(!previous->isVisible());
        headset->setProperty("anyDeviceFound", false);
        QCOMPARE(systemTray->property("tooltip").toString(), QStringLiteral("QontrolPanel"));
        QVERIFY(!actionFor(menu, "headset")->isVisible());
        QSignalSpy showSettings(systemTray.get(), SIGNAL(settingsWindowRequested()));
        actionFor(menu, "settings")->trigger();
        QCOMPARE(showSettings.size(), 1);
    }
};

QTEST_MAIN(TrayMenuTests)
#include "traymenu_tests.moc"
