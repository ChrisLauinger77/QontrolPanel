#include <QtTest>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <memory>
#include <windows.h>
#include "usersettings.h"

namespace {
constexpr auto Module = "ChrisLauinger77.QontrolPanel";
QUrl sourceUrl(const QString& path)
{
    return QUrl::fromLocalFile(QStringLiteral(QONTROLPANEL_SOURCE_DIR "/") + path);
}

QObject* findObject(QObject* root, const char* property, const QVariant& value)
{
    for (auto* child : root->findChildren<QObject*>()) {
        if (child->property(property) == value)
            return child;
    }
    return nullptr;
}

QObject* editor(QObject* pane, const QString& title, const char* property)
{
    auto* card = findObject(pane, "title", title);
    if (!card)
        return nullptr;
    for (auto* child : card->findChildren<QObject*>()) {
        if (qobject_cast<QQuickItem*>(child) && child->metaObject()->indexOfProperty(property) >= 0)
            return child;
    }
    return nullptr;
}

bool edit(QObject* control, const char* property, const QVariant& value, const QByteArray& signal)
{
    if (signal == "clicked") {
        auto* button = control;
        if (button->metaObject()->indexOfMethod("click()") < 0) {
            button = nullptr;
            for (auto* child : control->findChildren<QObject*>()) {
                if (child->metaObject()->indexOfMethod("click()") >= 0) {
                    button = child;
                    break;
                }
            }
        }
        return button && QMetaObject::invokeMethod(button, "click");
    }
    if (!control->setProperty(property, value))
        return false;
    if (signal == "activated")
        return QMetaObject::invokeMethod(control, signal.constData(), Q_ARG(int, value.toInt()));
    return QMetaObject::invokeMethod(control, signal.constData());
}
}

class SettingsUiTests : public QObject
{
    Q_OBJECT
    QTemporaryDir m_preferences;
    QString m_preferencesPath;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQuickWindow> m_window;
    std::unique_ptr<QObject> m_pane;
    int m_audioType = -1;

    QObject* loadPane(const QString& name)
    {
        m_window = std::make_unique<QQuickWindow>();
        m_window->resize(1100, 1000);
        m_engine = std::make_unique<QQmlEngine>();
        QQmlComponent component(m_engine.get(), sourceUrl("qml/SettingsPane/" + name + ".qml"));
        if (component.isError())
            qWarning().noquote() << component.errorString();
        m_pane.reset(component.create());
        if (auto* item = qobject_cast<QQuickItem*>(m_pane.get())) {
            item->setParentItem(m_window->contentItem());
            item->setSize(m_window->size());
        }
        m_window->show();
        QCoreApplication::processEvents();
        return m_pane.get();
    }

    HANDLE lockPreferences()
    {
        return CreateFileW(reinterpret_cast<LPCWSTR>(m_preferencesPath.utf16()), GENERIC_READ,
                           FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }

    QObject* audio() { return m_engine->singletonInstance<QObject*>(m_audioType); }

private slots:
    void initTestCase()
    {
        QVERIFY(m_preferences.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_preferences.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_preferences.path());
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
        settings.setValue("chatMixEnabled", false);
        settings.setValue("mediaOverlayPosition", 8);
        settings.sync();
        QCOMPARE(settings.status(), QSettings::NoError);
        m_preferencesPath = settings.fileName();
        QCOMPARE(UserSettings::instance()->mediaOverlayPosition(), 7);
        qmlRegisterSingletonType<UserSettings>(Module, 1, 0, "UserSettings", UserSettings::create);
        for (const auto* name : {"Card", "CustomScrollView", "CustomComboBox", "LabeledSwitch", "NFSlider"})
            qmlRegisterType(sourceUrl("qml/Common/" + QString::fromLatin1(name) + ".qml"), Module, 1, 0, name);
        for (const auto* name : {"Constants", "Utils", "StartupShortcutBridge", "HeadsetControlBridge", "Updater", "KeyboardShortcutManager"})
            qmlRegisterSingletonType(sourceUrl("tests/qml/SettingsUiStubs.qml"), Module, 1, 0, name);
        qmlRegisterSingletonType(sourceUrl("qml/Singletons/Context.qml"), Module, 1, 0, "Context");
        m_audioType = qmlRegisterSingletonType(sourceUrl("tests/qml/SettingsUiStubs.qml"), Module, 1, 0, "AudioBridge");
    }

    void init()
    {
        auto* settings = UserSettings::instance();
        settings->setActivateChatmix(true);
        settings->setChatMixEnabled(true);
        settings->setChatMixValue(50);
        settings->setEnableMediaOverlay(true);
    }

    void cleanup()
    {
        m_pane.reset();
        m_engine.reset();
        m_window.reset();
    }

    void shortcutCaptureWaitsForKey_data()
    {
        QTest::addColumn<QString>("target");
        QTest::addColumn<bool>("controlFirst");
        for (const auto* target : {"volume-up", "volume-down", "global"}) {
            QTest::newRow(qPrintable(QString(target) + "-control-first")) << QString(target) << true;
            QTest::newRow(qPrintable(QString(target) + "-shift-first")) << QString(target) << false;
        }
    }

    void shortcutCaptureWaitsForKey()
    {
        QFETCH(QString, target);
        QFETCH(bool, controlFirst);
        const bool global = target == "global";
        const bool volumeUp = target == "volume-up";
        QVERIFY(loadPane(global ? "ShortcutsPane" : "AppHotkeysPane"));
        auto* context = qmlContext(m_pane.get());
        QVERIFY(context);
        auto* dialog = context->objectForName(global ? "shortcutDialog" : "addHotkeyDialog");
        QVERIFY(dialog);
        QVERIFY(QMetaObject::invokeMethod(dialog, global ? "openForPanel" : "open"));
        QTRY_VERIFY(dialog->property("opened").toBool());
        auto* input = qobject_cast<QQuickItem*>(context->objectForName(
            global ? "inputRect" : volumeUp ? "volUpRect" : "volDownRect"));
        QVERIFY(input);
        const char* capturing = volumeUp ? "capturingUp" : "capturingDown";
        const char* keyProperty = global ? "tempKey" : volumeUp ? "volUpKey" : "volDownKey";
        const char* modsProperty = global ? "tempModifiers" : volumeUp ? "volUpMods" : "volDownMods";
        if (!global)
            QVERIFY(dialog->setProperty(capturing, true));
        input->forceActiveFocus();
        QTRY_VERIFY(input->hasActiveFocus());
        const int initialKey = dialog->property(keyProperty).toInt();
        const int initialMods = dialog->property(modsProperty).toInt();
        auto press = [&](Qt::Key key, Qt::KeyboardModifiers modifiers, bool repeat = false) {
            QKeyEvent event(QEvent::KeyPress, key, modifiers, QString{}, repeat);
            QCoreApplication::sendEvent(m_window.get(), &event);
        };
        const auto modifiers = Qt::ControlModifier | Qt::ShiftModifier;
        const QList<Qt::Key> modifierKeys = {
            controlFirst ? Qt::Key_Control : Qt::Key_Shift,
            controlFirst ? Qt::Key_Shift : Qt::Key_Control,
            Qt::Key_Alt, Qt::Key_Meta, Qt::Key_AltGr, Qt::Key_unknown};
        for (const auto key : modifierKeys) {
            press(key, modifiers);
            QCOMPARE(dialog->property(keyProperty).toInt(), initialKey);
            QCOMPARE(dialog->property(modsProperty).toInt(), initialMods);
            if (!global)
                QVERIFY(dialog->property(capturing).toBool());
        }
        press(Qt::Key_Up, modifiers, true);
        QCOMPARE(dialog->property(keyProperty).toInt(), initialKey);
        press(Qt::Key_Up, modifiers);
        QCOMPARE(dialog->property(keyProperty).toInt(), int(Qt::Key_Up));
        QCOMPARE(dialog->property(modsProperty).toInt(), int(modifiers));
        QVERIFY(findObject(dialog, "text", "Ctrl + Shift + Up"));
        if (!global)
            QVERIFY(!dialog->property(capturing).toBool());
        QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
        QTRY_VERIFY(!dialog->property("visible").toBool());
    }

    void overlayPositionBounds_data()
    {
        QTest::addColumn<int>("requested");
        QTest::addColumn<int>("accepted");
        QTest::newRow("below-first") << -1 << 0;
        QTest::newRow("first") << 0 << 0;
        QTest::newRow("last") << 7 << 7;
        QTest::newRow("off-by-one") << 8 << 7;
        QTest::newRow("above-last") << 100 << 7;
    }

    void overlayPositionBounds()
    {
        QFETCH(int, requested);
        QFETCH(int, accepted);
        auto* settings = UserSettings::instance();
        settings->setMediaOverlayPosition(1);
        QSignalSpy changed(settings, &UserSettings::mediaOverlayPositionChanged);
        settings->setMediaOverlayPosition(requested);
        QCOMPARE(settings->mediaOverlayPosition(), accepted);
        QCOMPARE(changed.size(), 1);
        QSettings saved(m_preferencesPath, QSettings::IniFormat);
        QCOMPARE(saved.value("mediaOverlayPosition").toInt(), accepted);
    }

    void rejectedEditor_data()
    {
        QTest::addColumn<QString>("pane");
        QTest::addColumn<QString>("title");
        QTest::addColumn<QByteArray>("setting");
        QTest::addColumn<QByteArray>("property");
        QTest::addColumn<QByteArray>("signal");
        QTest::addColumn<QVariant>("before");
        QTest::addColumn<QVariant>("after");
        QTest::addColumn<QVariant>("beforeUi");
        QTest::addColumn<QVariant>("afterUi");
        auto row = [](const char* pane, const char* title, const char* setting, const char* property,
                      const char* signal, QVariant before, QVariant after, QVariant beforeUi = {}, QVariant afterUi = {}) {
            QTest::newRow(setting) << QString::fromLatin1(pane) << QString::fromLatin1(title)
                << QByteArray(setting) << QByteArray(property) << QByteArray(signal) << before << after
                << (beforeUi.isValid() ? beforeUi : before) << (afterUi.isValid() ? afterUi : after);
        };
        row("MediaOverlayPane", "Enable media overlay", "enableMediaOverlay", "checked", "clicked", true, false);
        row("MediaOverlayPane", "Overlay size", "mediaOverlaySize", "currentIndex", "activated", 1, 2);
        row("AppearancePane", "Panel position", "panelPosition", "currentIndex", "activated", 1, 2);
        row("AppearancePane", "Panel theme", "panelStyle", "currentIndex", "activated", 0, 1);
        row("AppearancePane", "Tray icon theme", "trayIconTheme", "currentIndex", "activated", 0, 1);
        row("AppearancePane", "Tray icon style", "iconStyle", "currentIndex", "activated", 0, 1);
        row("AppearancePane", "Taskbar offset", "taskbarOffset", "value", "valueModified", 0, 20);
        row("AppearancePane", "Panel X margin", "xAxisMargin", "value", "valueModified", 12, 20);
        row("AppearancePane", "Panel Y margin", "yAxisMargin", "value", "valueModified", 12, 20);
        row("AppearancePane", "Show audio level", "showAudioLevel", "checked", "clicked", true, false);
        row("AppearancePane", "Settings page animations", "settingsAnimationsEnabled", "checked", "clicked", true, false);
        row("GeneralPane", "Settings startup page", "settingsStartupPage", "currentIndex", "activated", 0, 2);
        row("GeneralPane", "Show power action confirmation", "showPowerDialogConfirmation", "checked", "clicked", true, false);
        row("GeneralPane", "Power action confirmation timeout (seconds)", "powerDialogTimeout", "value", "valueModified", 30, 40);
        row("GeneralPane", "Slider wheel sensivity", "sliderWheelSensivity", "value", "valueModified", 2, 3);
        row("GeneralPane", "DDC/CI brightness update rate", "ddcciQueueDelay", "currentIndex", "activated", 500, 100, 0, 2);
        row("CommAppsPane", "ChatMix volume", "chatMixValue", "value", "wheelChanged", 50, 65);
        row("CommAppsPane", "Restored volume", "chatmixRestoreVolume", "value", "wheelChanged", 80, 60);
        row("HeadsetControlPane", "Notification on low battery", "headsetcontrolLowBatteryThreshold", "value", "valueModified", 25, 20);
        row("HeadsetControlPane", "Notification on low battery", "enableNotifications", "checked", "clicked", false, true);
        row("HeadsetControlPane", "Show battery status in panel footer", "displayBatteryFooter", "checked", "clicked", true, false);
        row("HeadsetControlPane", "Equalizer Preset", "headsetcontrolEqualizerPreset", "currentIndex", "activated", 0, 1);
        row("HeadsetControlPane", "Inactive time (minutes)", "headsetcontrolInactiveTime", "value", "wheelChanged", 30, 40);
        row("HeadsetControlPane", "Lights", "headsetcontrolLights", "checked", "clicked", true, false);
        row("HeadsetControlPane", "Rotate-to-Mute", "headsetcontrolRotateToMute", "checked", "clicked", true, false);
        row("HeadsetControlPane", "Sidetone", "headsetcontrolSidetone", "value", "wheelChanged", 0, 40);
        row("HeadsetControlPane", "Voice Prompts", "headsetcontrolVoicePrompts", "checked", "clicked", true, false);
        row("HeadsetControlPane", "Fetch rate (seconds)", "headsetcontrolFetchRate", "value", "valueModified", 60, 120);
    }

    void rejectedEditor()
    {
        QFETCH(QString, pane); QFETCH(QString, title); QFETCH(QByteArray, setting);
        QFETCH(QByteArray, property); QFETCH(QByteArray, signal);
        QFETCH(QVariant, before); QFETCH(QVariant, after); QFETCH(QVariant, beforeUi); QFETCH(QVariant, afterUi);
        auto* settings = UserSettings::instance();
        QVERIFY(settings->setProperty(setting.constData(), before));
        QVERIFY(loadPane(pane));
        auto* control = editor(m_pane.get(), title, property.constData());
        QVERIFY2(control, qPrintable(title));
        QCOMPARE(control->property(property.constData()), beforeUi);
        const auto metaProperty = settings->metaObject()->property(settings->metaObject()->indexOfProperty(setting.constData()));
        QSignalSpy changed(settings, metaProperty.notifySignal());
        QSignalSpy failed(settings, &UserSettings::saveFailed);
        {
            const HANDLE lock = lockPreferences();
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            QVERIFY(edit(control, property.constData(), afterUi, signal));
            QCOMPARE(settings->property(setting.constData()), before);
            QCOMPARE(control->property(property.constData()), beforeUi);
            QCOMPARE(changed.size(), 0);
            QCOMPARE(failed.size(), 1);
            if (pane == "CommAppsPane") {
                QCOMPARE(audio()->property("applyCount").toInt(), 0);
                QCOMPARE(audio()->property("restoreCount").toInt(), 0);
            }
        }
        // Windows can briefly retain another sharing lock after our handle closes.
        QTRY_VERIFY((settings->property(setting.constData()) == after
                     || edit(control, property.constData(), afterUi, signal))
                    && settings->property(setting.constData()) == after);
        QCOMPARE(settings->property(setting.constData()), after);
        QCOMPARE(control->property(property.constData()), afterUi);
        QCOMPARE(changed.size(), 1);
        QVERIFY(settings->setProperty(setting.constData(), before));
        QCOMPARE(control->property(property.constData()), beforeUi);
    }

    void rejectedChatMixToggle_data()
    {
        QTest::addColumn<bool>("activatedToggle");
        QTest::addColumn<bool>("before");
        QTest::newRow("enable") << false << false;
        QTest::newRow("disable") << false << true;
        QTest::newRow("deactivate") << true << true;
    }

    void rejectedOverlayPosition()
    {
        auto* settings = UserSettings::instance();
        settings->setMediaOverlayPosition(0);
        QVERIFY(loadPane("MediaOverlayPane"));
        QList<QObject*> buttons;
        for (auto* child : m_pane->findChildren<QObject*>()) {
            if (child->inherits("QQuickRadioButton"))
                buttons.append(child);
        }
        QCOMPARE(buttons.size(), 8);
        QVERIFY(buttons[0]->property("checked").toBool());
        {
            const HANDLE lock = lockPreferences();
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            QVERIFY(QMetaObject::invokeMethod(buttons[1], "click"));
            QCOMPARE(settings->mediaOverlayPosition(), 0);
            QVERIFY(buttons[0]->property("checked").toBool());
            QVERIFY(!buttons[1]->property("checked").toBool());
        }
        QVERIFY(QMetaObject::invokeMethod(buttons[1], "click"));
        QCOMPARE(settings->mediaOverlayPosition(), 1);
        QVERIFY(!buttons[0]->property("checked").toBool());
        QVERIFY(buttons[1]->property("checked").toBool());
        settings->setMediaOverlayPosition(2);
        QVERIFY(!buttons[1]->property("checked").toBool());
        QVERIFY(buttons[2]->property("checked").toBool());
    }

    void rejectedChatMixToggle()
    {
        QFETCH(bool, activatedToggle); QFETCH(bool, before);
        auto* settings = UserSettings::instance();
        settings->setChatMixEnabled(before);
        QVERIFY(loadPane("CommAppsPane"));
        auto* control = editor(m_pane.get(), activatedToggle ? "Activate ChatMix" : "Use ChatMix volume", "checked");
        QVERIFY(control);
        {
            const HANDLE lock = lockPreferences();
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            QVERIFY(edit(control, "checked", !before, "clicked"));
            QCOMPARE(control->property("checked").toBool(), before);
            QCOMPARE(settings->chatMixEnabled(), before);
            QVERIFY(settings->activateChatmix());
            QCOMPARE(audio()->property("applyCount").toInt(), 0);
            QCOMPARE(audio()->property("restoreCount").toInt(), 0);
        }
        QVERIFY(edit(control, "checked", !before, "clicked"));
        QCOMPARE(settings->chatMixEnabled(), !before);
        QCOMPARE(audio()->property("applyCount").toInt(), before ? 0 : 1);
        QCOMPARE(audio()->property("restoreCount").toInt(), before ? 1 : 0);
    }

    void rejectedActivation_data()
    {
        QTest::addColumn<bool>("partial");
        QTest::newRow("first-write") << false;
        QTest::newRow("second-write") << true;
    }

    void rejectedActivation()
    {
        QFETCH(bool, partial);
        auto* settings = UserSettings::instance();
        settings->setActivateChatmix(false);
        settings->setChatMixEnabled(false);
        QVERIFY(loadPane("CommAppsPane"));
        auto* control = editor(m_pane.get(), "Activate ChatMix", "checked");
        QVERIFY(control);
        QVERIFY(edit(control, "checked", true, "clicked"));
        QCOMPARE(control->property("checked").toBool(), false);
        auto* dialog = findObject(m_pane.get(), "title", "Enable ChatMix Warning");
        QVERIFY(dialog);
        auto* button = findObject(dialog, "text", "Activate");
        QVERIFY(button);
        HANDLE lock = partial ? INVALID_HANDLE_VALUE : lockPreferences();
        if (!partial)
            QVERIFY(lock != INVALID_HANDLE_VALUE);
        const auto connection = connect(settings, &UserSettings::activateChatmixChanged, this, [&] {
            if (partial)
                lock = lockPreferences();
        });
        {
            const auto release = qScopeGuard([&] {
                disconnect(connection);
                if (lock != INVALID_HANDLE_VALUE)
                    CloseHandle(lock);
            });
            QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            QCOMPARE(settings->activateChatmix(), partial);
            QVERIFY(!settings->chatMixEnabled());
            QVERIFY(dialog->property("visible").toBool());
            QCOMPARE(audio()->property("applyCount").toInt(), 0);
            QCOMPARE(audio()->property("restoreCount").toInt(), 0);
        }
        QVERIFY(QMetaObject::invokeMethod(button, "clicked"));
        QVERIFY(settings->activateChatmix());
        QVERIFY(settings->chatMixEnabled());
        QTRY_VERIFY(!dialog->property("visible").toBool());
        QCOMPARE(audio()->property("applyCount").toInt(), 1);
        QCOMPARE(audio()->property("appliedVolume").toInt(), settings->chatMixValue());
    }
};

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication application(argc, argv);
    SettingsUiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "settings_ui_tests.moc"
