#include <QtTest>
#include <QSignalSpy>
#include <QAbstractItemModelTester>
#include "audiomodels.h"
#include "sessionindices.h"
#include "headsetlevelpolicy.h"
#include "headsetequalizer.h"
#include <limits>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <atomic>
#include <thread>
#include <vector>
#include "jsonstore.h"
#include "logmanager.h"
#include "workerthreads.h"
#include "replybatch.h"
#include "updatestaging.h"
#ifdef Q_OS_WIN
#include "keyboardshortcutmanager.h"
#include "usersettings.h"
#ifdef QONTROLPANEL_AUDIO_POLICY_TESTS
#include "audiobridge.h"
#include "headsetcontrolmonitor.h"
#include "headsetcontrolbridge.h"
#endif
#ifdef QONTROLPANEL_LANGUAGE_TESTS
#include "languagebridge.h"
#include "updater.h"
#endif
#include <QScopeGuard>
#include <QStandardPaths>
#include <QUuid>
#include <QSettings>
#include <memory>
#endif

class FakeWorker : public QObject
{
    Q_OBJECT
public:
    std::atomic<int>* operations;
    std::atomic<bool>* cleaned;
    std::atomic<bool>* destroyed;
    ~FakeWorker() override { destroyed->store(QThread::currentThread() == thread()); }
public slots:
    void cleanup() { cleaned->store(QThread::currentThread() == thread() && operations->load() == 20); }
};

class SynchronousReply : public QNetworkReply
{
public:
    int* aborted;
    void abort() override
    {
        ++*aborted;
        emit finished();
    }
    qint64 readData(char*, qint64) override { return -1; }
};

class ReliabilityTests : public QObject
{
    Q_OBJECT
#ifdef Q_OS_WIN
    std::unique_ptr<KeyboardShortcutManager> m_shortcuts;
#ifdef QONTROLPANEL_AUDIO_POLICY_TESTS
    std::unique_ptr<AudioBridge> m_audio;
#endif
    QString m_originalApplicationName;
    QString m_shortcutDataDirectory;
    bool m_originalTestMode = false;
    QTemporaryDir m_preferencesDirectory;
    QSettings::Format m_originalSettingsFormat = QSettings::NativeFormat;
    QString m_preferencesPath;

    static bool shortcutIsAvailable(UINT key)
    {
        bool available = false;
        std::thread([&] {
            constexpr int probeId = 0x3fff;
            available = RegisterHotKey(nullptr, probeId, MOD_CONTROL | MOD_ALT | MOD_SHIFT, key) != 0;
            if (available)
                UnregisterHotKey(nullptr, probeId);
        }).join();
        return available;
    }
#endif
private slots:
    void equalizerCurveClampingAndValidation()
    {
        const HeadsetEqualizer::Layout layout{10, 0, 0.5, -12, 12};
        const QVariantList input{-100, -11.8, -1.24, -0.25, 0, 0.24, 0.26, 1.25, 11.9, 100};
        const QVariantList expected{-12.0, -12.0, -1.0, 0.0, 0.0, 0.0, 0.5, 1.5, 12.0, 12.0};
        QCOMPARE(HeadsetEqualizer::clamp(input, layout), expected);
        QVERIFY(HeadsetEqualizer::clamp({0, 0}, layout).isEmpty());
        auto invalid = input;
        for (const auto& value : {QVariant(std::numeric_limits<double>::quiet_NaN()),
                                 QVariant(std::numeric_limits<double>::infinity()), QVariant("1"), QVariant(true)}) {
            invalid[0] = value;
            QVERIFY(HeadsetEqualizer::clamp(invalid, layout).isEmpty());
        }
        auto badLayout = layout;
        badLayout.step = 0;
        QVERIFY(HeadsetEqualizer::clamp(input, badLayout).isEmpty());
        badLayout = layout;
        badLayout.count = 65;
        QVERIFY(!badLayout.valid());
        badLayout = layout;
        badLayout.baseline = 13;
        QVERIFY(!badLayout.valid());
        const HeadsetEqualizer::Layout offset{2, 5, 0.5, 2, 8};
        QCOMPARE(HeadsetEqualizer::clamp({1, 5.3}, offset), QVariantList({2.0, 5.5}));
    }

    void equalizerStorageSchemaAndModel()
    {
        const QVariantMap entry{{"custom", true}, {"preset", 1}, {"bands", QVariantList{0.5, -2.0}}};
        const QVariantMap entries{{"1234:5678:Headset A", entry}, {"1234:5679:Headset B", entry}};
        QCOMPARE(HeadsetEqualizer::stored(entries).value(), entries);
        auto invalidEntry = entry;
        invalidEntry["bands"] = QVariantList{};
        QVERIFY(!HeadsetEqualizer::stored(QVariantMap{{"device", invalidEntry}}));
        invalidEntry = entry;
        invalidEntry["bands"] = QVariantList{std::numeric_limits<double>::infinity()};
        QVERIFY(!HeadsetEqualizer::stored(QVariantMap{{"device", invalidEntry}}));
        invalidEntry = entry;
        invalidEntry["preset"] = -1;
        QVERIFY(!HeadsetEqualizer::stored(QVariantMap{{"device", invalidEntry}}));
        QVERIFY(!HeadsetEqualizer::stored(QVariantList{}));
        QVERIFY(!HeadsetEqualizer::stored(QVariantMap{{"", entry}}));
        HeadsetEqualizerModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setBands({0.0, 1.0});
        const QPersistentModelIndex band = model.index(0);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setBands({0.5, 1.0});
        QCOMPARE(reset.size(), 0);
        QCOMPARE(changed.size(), 1);
        QVERIFY(band.isValid());
        QCOMPARE(band.data(Qt::UserRole).toDouble(), 0.5);
        model.setBands({0.5, 1.0});
        QCOMPARE(changed.size(), 1);
        model.setBands({0.0, 0.0}, true);
        QCOMPARE(reset.size(), 1);
        QVERIFY(!band.isValid());
    }

    void headsetLevelAdoptVersusApply_data()
    {
        QTest::addColumn<bool>("readable");
        QTest::addColumn<int>("observed");
        QTest::addColumn<int>("lastApplied");
        QTest::addColumn<int>("desired");
        QTest::addColumn<bool>("userRequested");
        QTest::addColumn<int>("action");
        using A = HeadsetLevelPolicy::Action;
        QTest::newRow("first-read-keeps-device-level") << true << 85 << -1 << 0 << false << int(A::Adopt);
        QTest::newRow("external-change-wins") << true << 32 << 85 << 85 << false << int(A::Adopt);
        QTest::newRow("external-change-cancels-pending-retry") << true << 32 << 85 << 64 << true << int(A::Adopt);
        QTest::newRow("matching-read-allows-user-edit") << true << 85 << 85 << 64 << true << int(A::Apply);
        QTest::newRow("matching-level-needs-no-write") << true << 85 << 85 << 85 << false << int(A::None);
        QTest::newRow("failed-initial-read-does-not-restore-zero") << true << -1 << -1 << 0 << false << int(A::None);
        QTest::newRow("invalid-read-does-not-restore-zero") << true << 129 << -1 << 0 << false << int(A::None);
        QTest::newRow("explicit-edit-without-read") << true << -1 << -1 << 64 << true << int(A::Apply);
        QTest::newRow("write-only-retains-restoration") << false << -1 << -1 << 0 << false << int(A::Apply);
        QTest::newRow("write-only-skips-duplicate") << false << -1 << 64 << 64 << false << int(A::None);
        QTest::newRow("device-muted-to-zero") << true << 0 << 85 << 85 << false << int(A::Adopt);
        QTest::newRow("readable-unset-adopts") << true << 77 << -1 << -1 << false << int(A::Adopt);
        QTest::newRow("write-only-unset-skips") << false << -1 << -1 << -1 << false << int(A::None);
        QTest::newRow("unset-cancels-pending-write") << true << -1 << 77 << -1 << true << int(A::None);
        QTest::newRow("write-only-unset-with-baseline") << false << -1 << 77 << -1 << false << int(A::None);
    }

    void headsetLevelAdoptVersusApply()
    {
        QFETCH(bool, readable);
        QFETCH(int, observed);
        QFETCH(int, lastApplied);
        QFETCH(int, desired);
        QFETCH(bool, userRequested);
        QFETCH(int, action);
        QCOMPARE(int(HeadsetLevelPolicy::decide(readable, observed, lastApplied, desired, userRequested)), action);
    }
#ifdef Q_OS_WIN
    void initTestCase()
    {
        // Keep native hotkey tests completely separate from the user's preferences.
        QVERIFY(m_preferencesDirectory.isValid());
        m_originalSettingsFormat = QSettings::defaultFormat();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_preferencesDirectory.path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, m_preferencesDirectory.path());
        QSettings preferences(QSettings::IniFormat, QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
        preferences.setValue("enableDeviceManager", false);
        preferences.setValue("enableApplicationMixer", false);
        preferences.setValue("headsetcontrolMonitoring", false);
        preferences.setValue("globalShortcutsEnabled", true);
        preferences.setValue("autoUpdateTranslations", false);
        preferences.setValue("autoFetchForAppUpdates", false);
        preferences.sync();
        QCOMPARE(preferences.status(), QSettings::NoError);
        m_preferencesPath = preferences.fileName();
        m_originalApplicationName = QCoreApplication::applicationName();
        m_originalTestMode = QStandardPaths::isTestModeEnabled();
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName("QontrolPanelShortcutTests-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        m_shortcutDataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QVERIFY(!QDir(m_shortcutDataDirectory).exists());
    }
    void init()
    {
        // The singleton survives individual tests, including failed assertions. Reset it before
        // creating a manager, using keys separate from both the app defaults and F20-F23 below.
        auto* settings = UserSettings::instance();
        QSignalSpy failed(settings, &UserSettings::saveFailed);
        constexpr int modifiers = Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier;
        settings->setPanelShortcutKey(Qt::Key_F17);
        settings->setPanelShortcutModifiers(modifiers);
        settings->setChatMixShortcutKey(Qt::Key_F18);
        settings->setChatMixShortcutModifiers(modifiers);
        settings->setMicMuteShortcutKey(Qt::Key_F19);
        settings->setMicMuteShortcutModifiers(modifiers);
        settings->setGlobalShortcutsEnabled(true);
        settings->setLanguageIndex(0);
        settings->setRememberApplicationVolumes(false);
        QCOMPARE(failed.size(), 0);
    }
    void cleanup()
    {
#ifdef QONTROLPANEL_AUDIO_POLICY_TESTS
        m_audio.reset();
#endif
        m_shortcuts.reset();
        if (QDir(m_shortcutDataDirectory).exists())
            QVERIFY(QDir(m_shortcutDataDirectory).removeRecursively());
    }
    void cleanupTestCase()
    {
        QSettings::setDefaultFormat(m_originalSettingsFormat);
        QCoreApplication::setApplicationName(m_originalApplicationName);
        QStandardPaths::setTestModeEnabled(m_originalTestMode);
    }
#ifdef QONTROLPANEL_AUDIO_POLICY_TESTS
    void equalizerSettingsPersistPerDeviceAndKeepCustomCurve()
    {
        auto* settings = UserSettings::instance();
        settings->setHeadsetcontrolMonitoring(false);
        settings->setHeadsetcontrolEqualizers({});
        auto* bridge = HeadsetControlBridge::instance();
        const auto cleanup = qScopeGuard([&] {
            delete bridge;
            drainRetiredWorkerThreads();
        });
        HeadsetControlState state;
        state.anyDeviceFound = true;
        state.hasEqualizerCapability = true;
        state.hasEqualizerPresetsCapability = true;
        state.activeDeviceKey = "1234:5678:Headset A";
        state.equalizerLayout = {10, 0, 0.5, -12, 12};
        state.equalizerPresetNames = {"Flat", "Bass"};
        const QVariantList flat{0., 0., 0., 0., 0., 0., 0., 0., 0., 0.};
        const QVariantList bass{6., 4., 2., 0., 0., 0., 0., 0., 0., 0.};
        state.equalizerPresetCurves = {QVariant(flat), QVariant(bass)};
        const auto applyState = [&] {
            return QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                             Q_ARG(HeadsetControlState, state));
        };
        QVERIFY(applyState());
        QCOMPARE(bridge->equalizerBands()->rowCount(), 10);
        QVERIFY(settings->headsetcontrolEqualizers().isEmpty()); // Merely opening writes nothing.
        bridge->startEqualizerFromPreset(1);
        QVERIFY(bridge->customEqualizerEnabled());
        QCOMPARE(bridge->equalizerBands()->index(0, 0).data(Qt::UserRole).toDouble(), 6.0);
        bridge->setEqualizerBand(state.activeDeviceKey, 0, 50);
        QCOMPARE(bridge->equalizerBands()->index(0, 0).data(Qt::UserRole).toDouble(), 12.0);
        bridge->setEqualizerBand(state.activeDeviceKey, 1, 0.26);
        auto a = settings->headsetcontrolEqualizers().value(state.activeDeviceKey).toMap();
        QCOMPARE(a.value("bands").toList().at(1).toDouble(), 0.5);
        QSettings persisted(QSettings::IniFormat, QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
        const auto reloaded = HeadsetEqualizer::stored(persisted.value("headsetcontrolEqualizers"));
        QVERIFY(reloaded);
        QCOMPARE(*reloaded, settings->headsetcontrolEqualizers());
        QSignalSpy changes(settings, &UserSettings::headsetcontrolEqualizersChanged);
        bridge->setEqualizerBand(state.activeDeviceKey, 1, 0.3); // Same snapped curve.
        bridge->setEqualizerBand("old-device", 0, 1);
        bridge->setEqualizerBand(state.activeDeviceKey, 10, 1);
        bridge->setEqualizerBand(state.activeDeviceKey, 0, std::numeric_limits<double>::quiet_NaN());
        QCOMPARE(changes.size(), 0);
        bridge->setEqualizerPreset(0);
        QVERIFY(!bridge->customEqualizerEnabled());
        QCOMPARE(settings->headsetcontrolEqualizers().value(state.activeDeviceKey).toMap().value("bands"), a.value("bands"));
        bridge->setCustomEqualizerEnabled(true);
        QVERIFY(bridge->customEqualizerEnabled());
        state.activeDeviceKey = "1234:5679:Headset B";
        QVERIFY(applyState());
        QVERIFY(!bridge->customEqualizerEnabled());
        bridge->setEqualizerBand(state.activeDeviceKey, 0, -4);
        QCOMPARE(settings->headsetcontrolEqualizers().size(), 2);
        state.activeDeviceKey = "1234:5678:Headset A";
        QVERIFY(applyState());
        QCOMPARE(bridge->equalizerBands()->index(0, 0).data(Qt::UserRole).toDouble(), 12.0);
        QVERIFY(bridge->customEqualizerEnabled());
        state = {};
        QVERIFY(applyState());
        QCOMPARE(bridge->equalizerBands()->rowCount(), 0);
        QVERIFY(!bridge->hasEqualizerCapability());
    }

    void equalizerLayoutIsRefreshedOnSameDeviceAfterPreset_data()
    {
        QTest::addColumn<bool>("custom");
        QTest::newRow("preset-reveals-layout") << false;
        QTest::newRow("saved-curve-bootstraps-layout") << true;
    }

    void equalizerLayoutIsRefreshedOnSameDeviceAfterPreset()
    {
        QFETCH(bool, custom);
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        bool presetApplied = false;
        const headsetcontrol::Headset* firstDevice = nullptr;
        bool sameDevice = true;
        HeadsetControlMonitor monitor(nullptr, [&] {
            // Simulate metadata learned only on the current discovered handle.
            presetApplied = false;
            firstDevice = nullptr;
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        }, [&](const headsetcontrol::Headset& headset) -> std::optional<EqualizerInfo> {
            if (!firstDevice) firstDevice = &headset;
            sameDevice = sameDevice && firstDevice == &headset;
            return presetApplied ? headset.getEqualizerInfo() : std::nullopt;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        connect(&monitor, &HeadsetControlMonitor::equalizerApplied, &monitor,
                [&](bool curve) { if (!curve) presetApplied = true; });
        QSignalSpy writes(&monitor, &HeadsetControlMonitor::equalizerApplied);
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        monitor.startMonitoring();
        const auto key = monitor.snapshot().activeDeviceKey;
        QCOMPARE(monitor.snapshot().equalizerLayout.count, 0);
        QVariantList bands{0., 0., 0., 0., 0., 0., 0., 0., 0., 0.};
        monitor.setDesiredSettings({{"equalizers", QVariantMap{{key, QVariantMap{
            {"custom", custom}, {"preset", 1}, {"bands", bands}}}}}});
        QVERIFY(sameDevice);
        QCOMPARE(monitor.snapshot().equalizerLayout.count, 10);
        QCOMPARE(writes.size(), custom ? 2 : 1);
        QCOMPARE(writes.first().at(0).toBool(), false);
        if (custom) QCOMPARE(writes.last().at(0).toBool(), true);
        else {
            monitor.requestRefresh();
            QCOMPARE(monitor.snapshot().equalizerLayout.count, 10); // Preset mode retains editor access.
            monitor.setDesiredSettings({{"equalizers", QVariantMap{{key, QVariantMap{
                {"custom", false}, {"preset", 1}, {"bands", bands}}}}}});
            QCOMPARE(monitor.snapshot().equalizerLayout.count, 10); // Reselecting the preset is harmless.
            QCOMPARE(writes.size(), 1);
            // Also cover choosing a preset first, then switching to custom mode
            // on a replacement handle that has not learned its layout yet.
            monitor.setDesiredSettings({{"equalizers", QVariantMap{{key, QVariantMap{
                {"custom", true}, {"preset", 1}, {"bands", bands}}}}}});
            QCOMPARE(writes.size(), 3);
            QCOMPARE(writes.last().at(0).toBool(), true);
        }
        for (int poll = 0; poll < 2; ++poll) {
            const int previousWrites = writes.size();
            monitor.requestRefresh();
            QVERIFY(sameDevice);
            QCOMPARE(monitor.snapshot().equalizerLayout.count, 10);
            QCOMPARE(writes.size(), previousWrites); // Metadata retention requires no polling writes.
            bands[0] = 0.5 * (poll + 1);
            monitor.setDesiredSettings({{"equalizers", QVariantMap{{key, QVariantMap{
                {"custom", true}, {"preset", 1}, {"bands", bands}}}}}});
            QCOMPARE(writes.size(), previousWrites + 2);
            QCOMPARE(writes.at(previousWrites).at(0).toBool(), false);
            QCOMPARE(writes.last().at(0).toBool(), true); // Bootstrap always restores the pending curve.
            monitor.setDesiredSettings({});
            QCOMPARE(writes.size(), previousWrites + 2); // Same-handle echoes remain deduplicated.
        }
    }

    void equalizerMonitorWritesChangedCurveOnceAndRetriesFailures()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        bool connected = true;
        HeadsetControlMonitor monitor(nullptr, [&] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return connected ? std::move(devices) : std::vector<headsetcontrol::Headset>{};
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        QSignalSpy writes(&monitor, &HeadsetControlMonitor::equalizerApplied);
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        monitor.startMonitoring();
        const auto state = monitor.snapshot();
        QVERIFY(state.hasEqualizerCapability);
        QCOMPARE(state.equalizerLayout.count, 10);
        QCOMPARE(state.equalizerLayout.minimum, -12.0);
        QCOMPARE(state.equalizerLayout.maximum, 12.0);
        QCOMPARE(state.equalizerLayout.step, 0.5);
        QCOMPARE(writes.size(), 0);
        QVariantList curve{99., -99., 0.26, 0., 0., 0., 0., 0., 0., 0.};
        QVariantMap entry{{"custom", true}, {"preset", 0}, {"bands", curve}};
        auto send = [&] {
            monitor.setDesiredSettings({{"equalizers", QVariantMap{{state.activeDeviceKey, entry}}}});
        };
        send();
        QCOMPARE(writes.size(), 1);
        QCOMPARE(writes.last().at(0).toBool(), true);
        send();
        monitor.requestRefresh();
        QCOMPARE(writes.size(), 1); // Polling/settings echoes never replay an acknowledged curve.
        curve[0] = 12.0; curve[1] = -12.0; curve[2] = 0.5;
        entry["bands"] = curve;
        send();
        QCOMPARE(writes.size(), 1); // Clamping happens before acknowledgement comparison.
        curve[3] = 2.0; entry["bands"] = curve;
        send();
        QCOMPARE(writes.size(), 2);
        entry["custom"] = false;
        send();
        QCOMPARE(writes.size(), 3);
        QCOMPARE(writes.last().at(0).toBool(), false);
        entry["custom"] = true;
        send();
        QCOMPARE(writes.size(), 4); // Returning to the retained curve reapplies it.
        const auto workingEntry = entry;
        entry["bands"] = QVariantList{0.};
        send();
        QCOMPARE(writes.size(), 4);
        QVERIFY(errors.last().at(0).toString().contains("equalizerCurve"));
        entry = workingEntry;
        headsetcontrol::setTestProfile(1);
        curve[4] = -2.0; entry["bands"] = curve;
        for (int i = 0; i < 5; ++i) send();
        QCOMPARE(writes.size(), 4);
        QVERIFY(errors.last().at(0).toString().contains("retry limit"));
        headsetcontrol::setTestProfile(3);
        monitor.requestRefresh();
        QCOMPARE(writes.size(), 5);
        QVERIFY(errors.last().at(0).toString().isEmpty());
        connected = false;
        monitor.requestRefresh();
        QVERIFY(monitor.snapshot().activeDeviceKey.isEmpty());
        QCOMPARE(monitor.snapshot().equalizerLayout.count, 0);
        connected = true;
        monitor.requestRefresh();
        QCOMPARE(writes.size(), 6); // Restore once when the active device returns.
    }

    void sidetoneBridgeRetainsLatestRequestUntilAcknowledged()
    {
        auto* settings = UserSettings::instance();
        settings->setHeadsetcontrolMonitoring(false);
        settings->setHeadsetcontrolSidetone(64);
        auto* bridge = HeadsetControlBridge::instance();
        const auto cleanup = qScopeGuard([&] {
            delete bridge;
            drainRetiredWorkerThreads();
        });
        // Keep queued worker replies pending while simulating a burst of user input.
        for (int level = 65; level <= 69; ++level)
            bridge->setSidetone(level);
        QCOMPARE(bridge->pendingSidetoneLevel(), 69);
        HeadsetControlState state;
        state.anyDeviceFound = true;
        state.sidetoneLevel = 65;
        state.sidetoneRequestRevision = 1;
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingSidetoneLevel(), 69);
        state.sidetoneLevel = 69;
        state.sidetoneRequestRevision = 5;
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingSidetoneLevel(), -1);
        QCOMPARE(bridge->sidetoneLevel(), 69);
        bridge->setSidetone(70);
        state.sidetoneRequestRevision = 6; // Latest command failed; reported level stays at 69.
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingSidetoneLevel(), -1);
        QCOMPARE(bridge->sidetoneLevel(), 69);
        bridge->setSidetone(71);
        state.sidetoneRequestRevision = 7;
        state.sidetoneLevel = 72; // Device quantisation wins once the command completes.
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingSidetoneLevel(), -1);
        QCOMPARE(bridge->sidetoneLevel(), 72);
    }

    void headsetVolumeBridgeRetainsLatestRequestUntilAcknowledged()
    {
        auto* settings = UserSettings::instance();
        settings->setHeadsetcontrolMonitoring(false);
        settings->setHeadsetcontrolHeadsetVolume(64);
        auto* bridge = HeadsetControlBridge::instance();
        const auto cleanup = qScopeGuard([&] {
            delete bridge;
            drainRetiredWorkerThreads();
        });
        // Keep queued worker replies pending while simulating a burst of user input.
        for (int level = 65; level <= 69; ++level)
            bridge->setHeadsetVolume(level);
        QCOMPARE(bridge->pendingHeadsetVolumeLevel(), 69);
        HeadsetControlState state;
        state.anyDeviceFound = true;
        state.headsetVolumeLevel = 65;
        state.headsetVolumeRequestRevision = 1;
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingHeadsetVolumeLevel(), 69);
        state.headsetVolumeLevel = 69;
        state.headsetVolumeRequestRevision = 5;
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingHeadsetVolumeLevel(), -1);
        QCOMPARE(bridge->headsetVolumeLevel(), 69);
        bridge->setHeadsetVolume(70);
        state.headsetVolumeRequestRevision = 6; // Latest command failed; reported level stays at 69.
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingHeadsetVolumeLevel(), -1);
        QCOMPARE(bridge->headsetVolumeLevel(), 69);
        bridge->setHeadsetVolume(71);
        state.headsetVolumeRequestRevision = 7;
        state.headsetVolumeLevel = 72; // Device quantisation wins once the command completes.
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QCOMPARE(bridge->pendingHeadsetVolumeLevel(), -1);
        QCOMPARE(bridge->headsetVolumeLevel(), 72);
    }

    void batteryTimeSnapshotsNotifyOnChanges()
    {
        HeadsetControlDevice device;
        HeadsetControlState state;
        QCOMPARE(device.timeToFullMinutes, -1);
        QCOMPARE(device.timeToEmptyMinutes, -1);
        QCOMPARE(state.timeToFullMinutes, -1);
        QCOMPARE(state.timeToEmptyMinutes, -1);
        auto* bridge = HeadsetControlBridge::instance();
        const auto cleanup = qScopeGuard([&] {
            bridge->shutdown();
            delete bridge;
            drainRetiredWorkerThreads();
        });
        QCOMPARE(bridge->timeToFullMinutes(), -1);
        QCOMPARE(bridge->timeToEmptyMinutes(), -1);
        QSignalSpy full(bridge, &HeadsetControlBridge::timeToFullMinutesChanged);
        QSignalSpy empty(bridge, &HeadsetControlBridge::timeToEmptyMinutesChanged);
        auto publish = [&] {
            return QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                             Q_ARG(HeadsetControlState, state));
        };
        state.timeToFullMinutes = 60;
        QVERIFY(publish());
        QCOMPARE(bridge->property("timeToFullMinutes").toInt(), 60);
        QCOMPARE(full.size(), 1);
        QCOMPARE(empty.size(), 0);
        QVERIFY(publish());
        QCOMPARE(full.size(), 1);
        state.timeToEmptyMinutes = 302;
        QVERIFY(publish());
        QCOMPARE(bridge->property("timeToEmptyMinutes").toInt(), 302);
        QCOMPARE(full.size(), 1);
        QCOMPARE(empty.size(), 1);
        state = HeadsetControlState{};
        QVERIFY(publish());
        QCOMPARE(bridge->timeToFullMinutes(), -1);
        QCOMPARE(bridge->timeToEmptyMinutes(), -1);
        QCOMPARE(full.size(), 2);
        QCOMPARE(empty.size(), 2);
    }

    void limitedHeadsetTestProfile()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        int discoveries = 0;
        HeadsetControlMonitor monitor(nullptr, [&] {
            ++discoveries;
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        QCOMPARE(monitor.snapshot().testProfile, 3);
        monitor.setTestModeEnabled(true);
        monitor.startMonitoring();
        QVERIFY(monitor.snapshot().hasEqualizerCapability);
        QVERIFY(monitor.snapshot().hasHeadsetVolumeCapability);
        QCOMPARE(monitor.snapshot().equalizerLayout.count, 10);
        QSignalSpy profileChanged(&monitor, &HeadsetControlMonitor::testProfileChanged);
        monitor.setTestProfile(10);
        const auto state = monitor.snapshot();
        QCOMPARE(state.testProfile, 10);
        QCOMPARE(headsetcontrol::getTestProfile(), 10);
        QCOMPARE(profileChanged.size(), 1);
        QVERIFY(state.anyDeviceFound);
        QVERIFY(state.hasSidetoneCapability);
        QVERIFY(state.hasLightsCapability);
        QCOMPARE(state.batteryLevel, 42);
        QCOMPARE(state.batteryStatus, QString("BATTERY_AVAILABLE"));
        const auto devices = monitor.getCachedDevices();
        QCOMPARE(devices.size(), 1);
        QCOMPARE(devices.first().capabilities.size(), 3);
        QVERIFY(devices.first().capabilities.contains("CAP_SIDETONE"));
        QVERIFY(devices.first().capabilities.contains("CAP_LIGHTS"));
        QVERIFY(devices.first().capabilities.contains("CAP_BATTERY_STATUS"));
        QVERIFY(!state.hasEqualizerCapability);
        QVERIFY(!state.hasEqualizerPresetsCapability);
        QVERIFY(!state.hasHeadsetVolumeCapability);
        QVERIFY(!state.canReadHeadsetVolume);
        QVERIFY(!state.hasAncCapability);
        QVERIFY(!state.hasChatMixCapability);
        QVERIFY(!state.hasVoicePromptsCapability);
        QVERIFY(!state.hasInactiveTimeCapability);
        QCOMPARE(state.equalizerLayout.count, 0);
        QVERIFY(state.equalizerPresetNames.isEmpty());
        QCOMPARE(state.sidetoneLevel, -1);
        QCOMPARE(state.headsetVolumeLevel, -1);
        QCOMPARE(state.micMuted, -1);
        QCOMPARE(state.micAttached, -1);
        QCOMPARE(state.chatMix, -1);
        const int previousDiscoveries = discoveries;
        monitor.setTestProfile(10);
        QCOMPARE(discoveries, previousDiscoveries);
        QCOMPARE(profileChanged.size(), 1);
        monitor.setTestProfile(3);
        QVERIFY(monitor.snapshot().hasEqualizerCapability);
        QVERIFY(monitor.snapshot().hasHeadsetVolumeCapability);
        QCOMPARE(monitor.snapshot().equalizerLayout.count, 10);
        QCOMPARE(profileChanged.size(), 2);
        monitor.stopMonitoring();
        for (int unsupported : {8, 9, 11}) {
            monitor.setTestProfile(unsupported);
            QCOMPARE(monitor.snapshot().testProfile, 7);
        }
        monitor.setTestProfile(0);
        QCOMPARE(monitor.snapshot().testProfile, 1);
    }

    void batteryTimeTestProfilesAndReset()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        bool connected = true;
        HeadsetControlMonitor monitor(nullptr, [&] {
            auto devices = connected ? headsetcontrol::discover() : std::vector<headsetcontrol::Headset>{};
            // Never query or write a real device in this test.
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        QCOMPARE(monitor.snapshot().timeToFullMinutes, -1);
        QCOMPARE(monitor.snapshot().timeToEmptyMinutes, -1);
        monitor.setTestProfile(2);
        monitor.setTestModeEnabled(true);
        monitor.startMonitoring();
        QCOMPARE(monitor.snapshot().batteryLevel, 50);
        QCOMPARE(monitor.snapshot().batteryStatus, QString("BATTERY_CHARGING"));
        auto check = [&](int full, int empty) {
            const auto state = monitor.snapshot();
            const auto devices = monitor.getCachedDevices();
            return devices.size() == 1 && devices.first().timeToFullMinutes == full
                && devices.first().timeToEmptyMinutes == empty
                && state.timeToFullMinutes == full && state.timeToEmptyMinutes == empty;
        };
        QVERIFY(check(60, -1));
        monitor.setTestProfile(3); // Successful report without optional estimates must clear the old value.
        QVERIFY(check(-1, -1));
        monitor.setTestProfile(6);
        QVERIFY(check(-1, 720));
        monitor.setTestProfile(7);
        QVERIFY(check(-1, 72));
        monitor.setTestProfile(4); // Offline report clears the discharge estimate.
        QVERIFY(check(-1, -1));
        monitor.setTestProfile(2);
        QVERIFY(check(60, -1));
        monitor.setTestProfile(5); // Timeout clears the charging estimate.
        QVERIFY(check(-1, -1));
        monitor.setTestProfile(7);
        QVERIFY(check(-1, 72));
        monitor.setTestProfile(1); // Synthetic error profile remains selectable, with unknown estimates.
        QVERIFY(check(-1, -1));
        monitor.setTestProfile(2);
        QVERIFY(check(60, -1));
        connected = false;
        monitor.requestRefresh();
        QVERIFY(monitor.getCachedDevices().isEmpty());
        QCOMPARE(monitor.snapshot().timeToFullMinutes, -1);
        QCOMPARE(monitor.snapshot().timeToEmptyMinutes, -1);
        connected = true;
        monitor.requestRefresh();
        QVERIFY(check(60, -1));
        monitor.stopMonitoring();
        QCOMPARE(monitor.snapshot().timeToFullMinutes, -1);
        QCOMPARE(monitor.snapshot().timeToEmptyMinutes, -1);
    }

    void microphoneSnapshotsNotifyOnChanges()
    {
        HeadsetControlDevice device;
        HeadsetControlState state;
        QCOMPARE(device.micMuted, -1);
        QCOMPARE(device.micAttached, -1);
        QCOMPARE(state.micMuted, -1);
        QCOMPARE(state.micAttached, -1);
        auto* bridge = HeadsetControlBridge::instance();
        const auto cleanup = qScopeGuard([&] {
            bridge->shutdown();
            delete bridge;
            drainRetiredWorkerThreads();
        });
        QCOMPARE(bridge->micMuted(), -1);
        QCOMPARE(bridge->micAttached(), -1);
        QSignalSpy muted(bridge, &HeadsetControlBridge::micMutedChanged);
        QSignalSpy attached(bridge, &HeadsetControlBridge::micAttachedChanged);
        auto publish = [&] {
            return QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                             Q_ARG(HeadsetControlState, state));
        };
        state.anyDeviceFound = true;
        state.micMuted = 0;
        state.micAttached = 1;
        QVERIFY(publish());
        QCOMPARE(bridge->property("micMuted").toInt(), 0);
        QCOMPARE(bridge->property("micAttached").toInt(), 1);
        QCOMPARE(muted.size(), 1);
        QCOMPARE(attached.size(), 1);
        QVERIFY(publish());
        QCOMPARE(muted.size(), 1);
        QCOMPARE(attached.size(), 1);
        state.micMuted = 1;
        QVERIFY(publish());
        QCOMPARE(bridge->micMuted(), 1);
        QCOMPARE(muted.size(), 2);
        QCOMPARE(attached.size(), 1);
        state.micAttached = 0;
        QVERIFY(publish());
        QCOMPARE(bridge->micAttached(), 0);
        QCOMPARE(muted.size(), 2);
        QCOMPARE(attached.size(), 2);
        state = HeadsetControlState{};
        QVERIFY(publish());
        QCOMPARE(bridge->micMuted(), -1);
        QCOMPARE(bridge->micAttached(), -1);
        QCOMPARE(muted.size(), 3);
        QCOMPARE(attached.size(), 3);
    }

    void microphoneTestDeviceReadsOnlyOnReachablePolls()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        bool connected = true;
        int discoveries = 0;
        HeadsetControlMonitor monitor(nullptr, [&] {
            ++discoveries;
            auto devices = connected ? headsetcontrol::discover() : std::vector<headsetcontrol::Headset>{};
            // Never query or write a real device in this test.
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        QCOMPARE(monitor.snapshot().micMuted, -1);
        QCOMPARE(monitor.snapshot().micAttached, -1);
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        QSignalSpy muted(&monitor, &HeadsetControlMonitor::micMutedChanged);
        QSignalSpy attached(&monitor, &HeadsetControlMonitor::micAttachedChanged);
        monitor.startMonitoring();
        QCOMPARE(monitor.snapshot().micMuted, 0);
        QCOMPARE(monitor.snapshot().micAttached, 1);
        QCOMPARE(monitor.getCachedDevices().size(), 1);
        QCOMPARE(monitor.getCachedDevices().first().micMuted, 0);
        QCOMPARE(monitor.getCachedDevices().first().micAttached, 1);
        QCOMPARE(muted.size(), 1);
        QCOMPARE(attached.size(), 1);
        // Making further reads fail must not affect cached getters or desired-setting changes.
        headsetcontrol::setTestProfile(1);
        monitor.setDesiredSettings({{"inactiveTime", -1}});
        QCOMPARE(monitor.micMuted(), 0);
        QCOMPARE(monitor.micAttached(), 1);
        QCOMPARE(monitor.snapshot().micMuted, 0);
        QCOMPARE(monitor.getCachedDevices().first().micAttached, 1);
        QCOMPARE(discoveries, 1);
        QCOMPARE(muted.size(), 1);
        QCOMPARE(attached.size(), 1);
        auto* logger = LogManager::instance();
        logger->clearLogs();
        monitor.setTestProfile(1); // Battery read now fails; microphone requests must be skipped.
        QVERIFY(monitor.snapshot().anyDeviceFound); // Synthetic write-retry profile stays selectable.
        QCOMPARE(monitor.snapshot().micMuted, -1);
        QCOMPARE(monitor.snapshot().micAttached, -1);
        QCOMPARE(monitor.getCachedDevices().first().micMuted, -1);
        QCOMPARE(monitor.getCachedDevices().first().micAttached, -1);
        QCOMPARE(muted.size(), 2);
        QCOMPARE(attached.size(), 2);
        bool batteryFailed = false;
        for (const auto& log : logger->snapshot()) {
            const auto message = log.toMap().value("message").toString();
            batteryFailed |= message.contains("Failed to read battery");
            QVERIFY(!message.contains("Failed to read microphone mute status"));
            QVERIFY(!message.contains("Failed to read microphone attachment status"));
        }
        QVERIFY(batteryFailed);
        monitor.setTestProfile(3);
        QCOMPARE(monitor.snapshot().micMuted, 0);
        QCOMPARE(monitor.snapshot().micAttached, 1);
        connected = false;
        monitor.requestRefresh();
        QVERIFY(!monitor.snapshot().anyDeviceFound);
        QVERIFY(monitor.getCachedDevices().isEmpty());
        QCOMPARE(monitor.snapshot().micMuted, -1);
        QCOMPARE(monitor.snapshot().micAttached, -1);
        connected = true;
        monitor.requestRefresh();
        QCOMPARE(monitor.snapshot().micMuted, 0);
        QCOMPARE(monitor.snapshot().micAttached, 1);
        monitor.stopMonitoring();
        QCOMPARE(monitor.snapshot().micMuted, -1);
        QCOMPARE(monitor.snapshot().micAttached, -1);
    }

    void writeOnlyHeadsetSettings_data()
    {
        QTest::addColumn<QString>("key");
        QTest::addColumn<int>("maximum");
        QTest::addColumn<QByteArray>("capability");
        QTest::addColumn<QByteArray>("setter");
        QTest::addColumn<QByteArray>("setting");
        QTest::newRow("anc") << QString("anc") << 2 << QByteArray("hasAncCapability")
            << QByteArray("setAnc") << QByteArray("headsetcontrolAnc");
        QTest::newRow("ancStartupMode") << QString("ancStartupMode") << 3 << QByteArray("hasAncStartupModeCapability")
            << QByteArray("setAncStartupMode") << QByteArray("headsetcontrolAncStartupMode");
        QTest::newRow("micVolume") << QString("micVolume") << 128 << QByteArray("hasMicVolumeCapability")
            << QByteArray("setMicVolume") << QByteArray("headsetcontrolMicVolume");
        QTest::newRow("noiseFilter") << QString("noiseFilter") << 2 << QByteArray("hasNoiseFilterCapability")
            << QByteArray("setNoiseFilter") << QByteArray("headsetcontrolNoiseFilter");
        QTest::newRow("volumeLimiter") << QString("volumeLimiter") << 1 << QByteArray("hasVolumeLimiterCapability")
            << QByteArray("setVolumeLimiter") << QByteArray("headsetcontrolVolumeLimiter");
        QTest::newRow("micMuteLedBrightness") << QString("micMuteLedBrightness") << 3 << QByteArray("hasMicMuteLedBrightnessCapability")
            << QByteArray("setMicMuteLedBrightness") << QByteArray("headsetcontrolMicMuteLedBrightness");
        QTest::newRow("bluetoothWhenPoweredOn") << QString("bluetoothWhenPoweredOn") << 1 << QByteArray("hasBluetoothWhenPoweredOnCapability")
            << QByteArray("setBluetoothWhenPoweredOn") << QByteArray("headsetcontrolBluetoothWhenPoweredOn");
        QTest::newRow("bluetoothCallVolume") << QString("bluetoothCallVolume") << 100 << QByteArray("hasBluetoothCallVolumeCapability")
            << QByteArray("setBluetoothCallVolume") << QByteArray("headsetcontrolBluetoothCallVolume");
    }

    void writeOnlyHeadsetSettings()
    {
        QFETCH(QString, key); QFETCH(int, maximum); QFETCH(QByteArray, capability);
        QFETCH(QByteArray, setter); QFETCH(QByteArray, setting);
        auto* settings = UserSettings::instance();
        QVERIFY(settings->setProperty(setting.constData(), -1));
        auto* bridge = HeadsetControlBridge::instance();
        const auto bridgeCleanup = qScopeGuard([&] {
            bridge->shutdown();
            delete bridge;
            drainRetiredWorkerThreads();
        });
        QSignalSpy capabilities(bridge, &HeadsetControlBridge::capabilitiesChanged);
        // Deliver capability snapshots without starting native monitoring.
        HeadsetControlState state;
        state.hasMicVolumeCapability = key == "micVolume";
        state.hasAncCapability = key == "anc";
        state.hasAncStartupModeCapability = key == "ancStartupMode";
        state.hasNoiseFilterCapability = key == "noiseFilter";
        state.hasVolumeLimiterCapability = key == "volumeLimiter";
        state.hasMicMuteLedBrightnessCapability = key == "micMuteLedBrightness";
        state.hasBluetoothWhenPoweredOnCapability = key == "bluetoothWhenPoweredOn";
        state.hasBluetoothCallVolumeCapability = key == "bluetoothCallVolume";
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QVERIFY(bridge->property(capability.constData()).toBool());
        QCOMPARE(capabilities.size(), 1);
        QVERIFY(QMetaObject::invokeMethod(bridge, setter.constData(), Q_ARG(int, maximum + 1)));
        QCOMPARE(settings->property(setting.constData()).toInt(), maximum);
        QSettings persisted(QSettings::IniFormat, QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
        QCOMPARE(persisted.value(setting.constData()).toInt(), maximum);
        QVERIFY(settings->setProperty(setting.constData(), -2));
        QCOMPARE(settings->property(setting.constData()).toInt(), -1);
        persisted.sync();
        QCOMPARE(persisted.value(setting.constData()).toInt(), -1);

        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        HeadsetControlMonitor monitor(nullptr, [] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        monitor.setDesiredSettings({{key, -1}});
        monitor.startMonitoring();
        QVERIFY(monitor.snapshot().anyDeviceFound);
        QVERIFY(errors.last().at(0).toString().isEmpty());
        const auto snapshot = monitor.snapshot();
        if (key == "micVolume") QVERIFY(snapshot.hasMicVolumeCapability);
        if (key == "anc") QVERIFY(snapshot.hasAncCapability);
        if (key == "ancStartupMode") QVERIFY(snapshot.hasAncStartupModeCapability);
        if (key == "noiseFilter") QVERIFY(snapshot.hasNoiseFilterCapability);
        if (key == "volumeLimiter") QVERIFY(snapshot.hasVolumeLimiterCapability);
        if (key == "micMuteLedBrightness") QVERIFY(snapshot.hasMicMuteLedBrightnessCapability);
        if (key == "bluetoothWhenPoweredOn") QVERIFY(snapshot.hasBluetoothWhenPoweredOnCapability);
        if (key == "bluetoothCallVolume") QVERIFY(snapshot.hasBluetoothCallVolumeCapability);
        monitor.setDesiredSettings({{key, 0}}); // Explicit zero/off is a valid choice.
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.setDesiredSettings({{key, maximum}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.setDesiredSettings({{key, maximum + 1000}}); // Clamp before uint8 conversion/library validation.
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.setDesiredSettings({{key, -1}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.stopMonitoring();
        if (key == "micVolume") QVERIFY(!monitor.snapshot().hasMicVolumeCapability);
        if (key == "anc") QVERIFY(!monitor.snapshot().hasAncCapability);
        if (key == "ancStartupMode") QVERIFY(!monitor.snapshot().hasAncStartupModeCapability);
        if (key == "noiseFilter") QVERIFY(!monitor.snapshot().hasNoiseFilterCapability);
        if (key == "volumeLimiter") QVERIFY(!monitor.snapshot().hasVolumeLimiterCapability);
        if (key == "micMuteLedBrightness") QVERIFY(!monitor.snapshot().hasMicMuteLedBrightnessCapability);
        if (key == "bluetoothWhenPoweredOn") QVERIFY(!monitor.snapshot().hasBluetoothWhenPoweredOnCapability);
        if (key == "bluetoothCallVolume") QVERIFY(!monitor.snapshot().hasBluetoothCallVolumeCapability);
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, monitor.snapshot())));
        QVERIFY(!bridge->property(capability.constData()).toBool());
        QCOMPARE(capabilities.size(), 2);
    }

    void ancButtonModesWriteAllNonemptyCycles()
    {
        auto* settings = UserSettings::instance();
        settings->setHeadsetcontrolAncButtonModes(-1);
        auto* bridge = HeadsetControlBridge::instance();
        const auto bridgeCleanup = qScopeGuard([&] {
            bridge->shutdown();
            delete bridge;
            drainRetiredWorkerThreads();
        });
        HeadsetControlState state;
        state.hasAncButtonModesCapability = true;
        QSignalSpy capabilities(bridge, &HeadsetControlBridge::capabilitiesChanged);
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, state)));
        QVERIFY(bridge->hasAncButtonModesCapability());
        QCOMPARE(capabilities.size(), 1);
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        HeadsetControlMonitor monitor(nullptr, [] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        monitor.setDesiredSettings({{"ancButtonModes", -1}});
        monitor.startMonitoring();
        QVERIFY(monitor.snapshot().hasAncButtonModesCapability);
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        for (int mask = 1; mask <= 7; ++mask) {
            bridge->setAncButtonModes(mask);
            QCOMPARE(settings->headsetcontrolAncButtonModes(), mask);
            QSettings persisted(QSettings::IniFormat, QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
            QCOMPARE(persisted.value("headsetcontrolAncButtonModes").toInt(), mask);
            monitor.setDesiredSettings({{"ancButtonModes", mask}});
            QVERIFY(errors.last().at(0).toString().isEmpty());
        }
        for (int invalid : {0, -2, 8, 255}) {
            bridge->setAncButtonModes(invalid);
            QCOMPARE(settings->headsetcontrolAncButtonModes(), 7);
        }
        monitor.stopMonitoring();
        QVERIFY(!monitor.snapshot().hasAncButtonModesCapability);
        QVERIFY(QMetaObject::invokeMethod(bridge, "applyState", Qt::DirectConnection,
                                         Q_ARG(HeadsetControlState, monitor.snapshot())));
        QVERIFY(!bridge->hasAncButtonModesCapability());
        QCOMPARE(capabilities.size(), 2);
        bridge->setAncButtonModes(-1);
        QCOMPARE(settings->headsetcontrolAncButtonModes(), -1);
    }

    void writeOnlyHeadsetSkipsUnsetAndBoundsRetries_data()
    {
        QTest::addColumn<QString>("key");
        QTest::newRow("noiseFilter") << QString("noiseFilter");
        QTest::newRow("anc") << QString("anc");
        QTest::newRow("ancStartupMode") << QString("ancStartupMode");
        QTest::newRow("ancButtonModes") << QString("ancButtonModes");
    }

    void writeOnlyHeadsetSkipsUnsetAndBoundsRetries()
    {
        QFETCH(QString, key);
        const auto supported = [&](const HeadsetControlState& state) {
            if (key == "anc") return state.hasAncCapability;
            if (key == "ancStartupMode") return state.hasAncStartupModeCapability;
            if (key == "ancButtonModes") return state.hasAncButtonModesCapability;
            return state.hasNoiseFilterCapability;
        };
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        HeadsetControlMonitor monitor(nullptr, [] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        monitor.startMonitoring();
        QVERIFY(supported(monitor.snapshot()));
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        monitor.setDesiredSettings({{key, 2}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
        // Profile 1 must keep the test controls usable even though its battery read fails.
        monitor.setTestProfile(1);
        QVERIFY(monitor.snapshot().anyDeviceFound);
        QVERIFY(supported(monitor.snapshot()));
        QCOMPARE(monitor.snapshot().testProfile, 1);
        monitor.setDesiredSettings({{key, 2}});
        QVERIFY(errors.last().at(0).toString().isEmpty()); // Acknowledged values are not written twice.
        monitor.setDesiredSettings({{key, -1}});
        QVERIFY(errors.last().at(0).toString().isEmpty()); // Unset must not become off or uint8(255).
        monitor.setDesiredSettings({{key, 2}}); // Selecting the former value after unset must write again.
        QVERIFY(errors.last().at(0).toString().contains("Test error condition"));
        monitor.setDesiredSettings({{key, -1}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
        if (key == "ancButtonModes") {
            for (int invalid : {0, 8, 255}) {
                monitor.setDesiredSettings({{key, invalid}});
                QVERIFY(errors.last().at(0).toString().isEmpty()); // Invalid/empty cycles never reach the library.
            }
        }
        const int retryValue = key == "ancButtonModes" ? 7 : 0;
        for (int attempt = 0; attempt < 3; ++attempt) {
            // Full desired snapshots and unrelated edits must not reset this value's budget.
            monitor.setDesiredSettings({{key, retryValue}, {"inactiveTime", -1}});
            QVERIFY(errors.last().at(0).toString().contains("Test error condition"));
            QVERIFY(!errors.last().at(0).toString().contains("retry limit reached"));
        }
        monitor.setDesiredSettings({{key, retryValue}});
        QVERIFY(errors.last().at(0).toString().contains(key + ": retry limit reached"));
        monitor.setTestProfile(3);
        monitor.setDesiredSettings({{key, retryValue}});
        QVERIFY(errors.last().at(0).toString().contains(key + ": retry limit reached")); // No fourth write.
        monitor.requestRefresh(); // Manual refresh resets the budget and succeeds with profile 3.
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.setTestProfile(1);
        monitor.setDesiredSettings({{key, retryValue}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.setDesiredSettings({{key, 1}}); // A changed value receives a new budget.
        QVERIFY(errors.last().at(0).toString().contains("Test error condition"));
        monitor.setDesiredSettings({{key, -1}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
    }

    void sidetoneTestDeviceAdoptsAndStillAcceptsWrites()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        // Discovery may enumerate USB IDs, but only the synthetic device is queried or written.
        HeadsetControlMonitor monitor(nullptr, [] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        monitor.setDesiredSettings({{"sidetone", 0}});
        QSignalSpy adopted(&monitor, &HeadsetControlMonitor::sidetoneAdopted);
        monitor.startMonitoring();
        QCOMPARE(monitor.snapshot().sidetoneLevel, 85);
        QCOMPARE(adopted.size(), 1);
        QCOMPARE(adopted.last().at(0).toInt(), 85);
        monitor.setDesiredSettings({{"lights", true}});
        QCOMPARE(monitor.snapshot().sidetoneLevel, 85);
        QCOMPARE(adopted.size(), 1);
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        // Fail only the synthetic device's writes after connection, keeping its cached reachability.
        headsetcontrol::setTestProfile(1);
        monitor.setSidetone(32, 1);
        for (bool lights : {false, true})
            monitor.setDesiredSettings({{"lights", lights}});
        monitor.setDesiredSettings({{"lights", false}});
        QVERIFY(errors.last().at(0).toString().contains("sidetone: retry limit reached"));
        QCOMPARE(monitor.snapshot().sidetoneLevel, 85);
        // A new user action retries the same desired value, even after its budget is exhausted.
        monitor.setSidetone(32, 2);
        QVERIFY(errors.last().at(0).toString().contains("Test error condition"));
        QVERIFY(!errors.last().at(0).toString().contains("retry limit reached"));
        for (bool lights : {false, true})
            monitor.setDesiredSettings({{"lights", lights}});
        monitor.setDesiredSettings({{"lights", false}});
        QVERIFY(errors.last().at(0).toString().contains("sidetone: retry limit reached"));
        // Recovery also succeeds without selecting a different value or doing a refresh.
        headsetcontrol::setTestProfile(3);
        monitor.setSidetone(32, 4);
        QVERIFY(errors.last().at(0).toString().isEmpty());
        QCOMPARE(monitor.snapshot().sidetoneLevel, 32); // Successful library write acknowledgement.
        QCOMPARE(adopted.size(), 1);
        monitor.setDesiredSettings({{"lights", false}});
        QCOMPARE(monitor.snapshot().sidetoneLevel, 32);
        monitor.requestRefresh(); // The upstream test device always reads back 85.
        QCOMPARE(monitor.snapshot().sidetoneLevel, 85);
        QCOMPARE(adopted.size(), 2);
        QCOMPARE(adopted.last().at(1).toULongLong(), quint64(4));
        monitor.stopMonitoring();
        QCOMPARE(monitor.snapshot().sidetoneLevel, -1);
        monitor.startMonitoring();
        QCOMPARE(monitor.snapshot().sidetoneLevel, 85);
        QCOMPARE(adopted.size(), 3);
    }

    void headsetVolumeReadableAdoptsWheelChanges()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        bool connected = true;
        HeadsetControlMonitor monitor(nullptr, [&] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [&](const auto& headset) {
                return !connected || headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(3);
        monitor.setTestModeEnabled(true);
        monitor.setDesiredSettings({{"headsetVolume", 0}});
        QSignalSpy adopted(&monitor, &HeadsetControlMonitor::headsetVolumeAdopted);
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        monitor.startMonitoring();
        QVERIFY(monitor.snapshot().hasHeadsetVolumeCapability);
        QVERIFY(monitor.snapshot().canReadHeadsetVolume);
        constexpr int reported = 77; // Pinned test device: native step 15 of 0–21.
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        QCOMPARE(monitor.getCachedDevices().first().headsetVolumeLevel, reported);
        QCOMPARE(adopted.size(), 1); // Saved zero was adopted, not restored over the device.
        monitor.setDesiredSettings({{"lights", true}});
        QCOMPARE(adopted.size(), 1);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        // A failed command retains the last reading and is retried only three times.
        headsetcontrol::setTestProfile(1);
        monitor.setHeadsetVolume(32, 1);
        for (bool lights : {false, true})
            monitor.setDesiredSettings({{"lights", lights}});
        monitor.setDesiredSettings({{"lights", false}});
        QVERIFY(errors.last().at(0).toString().contains("headsetVolume: retry limit reached"));
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        // Manual refresh resets the retry budget before applying pending settings.
        headsetcontrol::setTestProfile(3);
        monitor.requestRefresh();
        // The reading still matches the original baseline, so the pending explicit edit applies.
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, 32);
        monitor.requestRefresh();
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        QCOMPARE(adopted.size(), 2);
        QCOMPARE(adopted.last().at(1).toULongLong(), quint64(1));
        monitor.setHeadsetVolume(64, 2);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, 64);
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.requestRefresh();
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        QCOMPARE(adopted.size(), 3);
        // A failed poll clears the reading. A later successful poll recovers it.
        monitor.setTestProfile(1);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1);
        QCOMPARE(monitor.getCachedDevices().first().headsetVolumeLevel, -1);
        monitor.setTestProfile(3);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        connected = false;
        monitor.requestRefresh();
        QVERIFY(!monitor.snapshot().hasHeadsetVolumeCapability);
        QVERIFY(!monitor.snapshot().canReadHeadsetVolume);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1);
        connected = true;
        monitor.requestRefresh();
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, reported);
        monitor.stopMonitoring();
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1);
        QVERIFY(!monitor.snapshot().canReadHeadsetVolume);
    }

    void headsetVolumeWriteOnlyWritesWithoutReadback()
    {
        const bool wasEnabled = headsetcontrol::isTestDeviceEnabled();
        const int previousProfile = headsetcontrol::getTestProfile();
        HeadsetControlMonitor monitor(nullptr, [] {
            auto devices = headsetcontrol::discover();
            std::erase_if(devices, [](const auto& headset) {
                return headset.vendorId() != 0xF00B || headset.productId() != 0xA00C;
            });
            return devices;
        });
        const auto cleanup = qScopeGuard([&] {
            monitor.stopMonitoring();
            headsetcontrol::enableTestDevice(wasEnabled);
            headsetcontrol::setTestProfile(previousProfile);
        });
        monitor.setTestProfile(6);
        monitor.setTestModeEnabled(true);
        monitor.setDesiredSettings({{"headsetVolume", -1}});
        QSignalSpy adopted(&monitor, &HeadsetControlMonitor::headsetVolumeAdopted);
        QSignalSpy errors(&monitor, &HeadsetControlMonitor::operationErrorChanged);
        monitor.startMonitoring();
        QVERIFY(monitor.snapshot().hasHeadsetVolumeCapability);
        QVERIFY(!monitor.snapshot().canReadHeadsetVolume);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1);
        for (int level : {0, 64, 128, 1000}) {
            monitor.setHeadsetVolume(level);
            QVERIFY(errors.last().at(0).toString().isEmpty());
            QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1); // A write ack is not a reading.
        }
        monitor.requestRefresh();
        QCOMPARE(monitor.getCachedDevices().first().headsetVolumeLevel, -1);
        QCOMPARE(adopted.size(), 0);
        monitor.setTestProfile(1); // Failed writes make acknowledgement and unset guards observable.
        monitor.setDesiredSettings({{"headsetVolume", 128}});
        QVERIFY(errors.last().at(0).toString().isEmpty()); // Already acknowledged; no duplicate write.
        monitor.setHeadsetVolume(-1);
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.setHeadsetVolume(128);
        QVERIFY(errors.last().at(0).toString().contains("Test error condition"));
        monitor.setHeadsetVolume(-1);
        monitor.setTestProfile(6);
        QVERIFY(errors.last().at(0).toString().isEmpty());
        // A saved level is restored for write-only hardware without an explicit command flag.
        monitor.setDesiredSettings({{"headsetVolume", 48}});
        QVERIFY(errors.last().at(0).toString().isEmpty());
        monitor.stopMonitoring();
        monitor.startMonitoring();
        QVERIFY(errors.last().at(0).toString().isEmpty());
        QVERIFY(!monitor.snapshot().canReadHeadsetVolume);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1);
        monitor.setTestProfile(1);
        monitor.setDesiredSettings({{"headsetVolume", 48}});
        QVERIFY(errors.last().at(0).toString().isEmpty()); // Restoration was acknowledged.
        monitor.setHeadsetVolume(-1);
        monitor.setHeadsetVolume(48);
        QVERIFY(errors.last().at(0).toString().contains("Test error condition"));
        monitor.setHeadsetVolume(-1);
        monitor.setTestProfile(3);
        QVERIFY(monitor.snapshot().canReadHeadsetVolume);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, 77);
        QCOMPARE(adopted.size(), 1);
        monitor.setTestProfile(6);
        QVERIFY(!monitor.snapshot().canReadHeadsetVolume);
        QCOMPARE(monitor.snapshot().headsetVolumeLevel, -1);
        QCOMPARE(adopted.size(), 1);
    }

    void audioPolicySaveFailure_data()
    {
        QTest::addColumn<QString>("policy");
        QTest::addColumn<QString>("operation");
        for (const QString& policy : {"commApps", "appRenames", "executableRenames", "appLocks",
                                      "deviceRenames", "deviceIcons", "backgroundMutedApps"}) {
            for (const QString& operation : {"add", "replace", "remove"}) {
                if (operation == "replace" && (policy == "commApps" || policy == "appLocks" || policy == "backgroundMutedApps"))
                    continue;
                QTest::newRow(qPrintable(policy + "-" + operation)) << policy << operation;
            }
        }
    }
    void audioPolicySaveFailure()
    {
        QFETCH(QString, policy);
        QFETCH(QString, operation);
        const QMap<QString, QString> files = {{"commApps", "commapps.json"}, {"appRenames", "apprenames.json"},
            {"executableRenames", "executablenames.json"}, {"appLocks", "applocks.json"},
            {"deviceRenames", "devicerenames.json"}, {"deviceIcons", "deviceicons.json"},
            {"backgroundMutedApps", "backgroundmute.json"}};
        QVERIFY(QDir().mkpath(m_shortcutDataDirectory));
        const QString path = m_shortcutDataDirectory + "/" + files.value(policy);
        QVERIFY(JsonStore::save(path, QJsonDocument(QJsonObject{{policy, QJsonArray{}}})));
        m_shortcuts.reset(KeyboardShortcutManager::instance());
        m_audio.reset(AudioBridge::instance());
        QVERIFY(!m_audio->isReady()); // No Core Audio or HID workers are started by these tests.
        AudioApplication app;
        app.id = "session";
        app.name = "Player";
        app.executableName = "Player";
        app.streamIndex = 0;
        AudioManager::instance()->applicationsChanged({app});
        AudioDevice device;
        device.id = "endpoint";
        device.name = "Player";
        m_audio->outputDevices()->setDevices({device});
        auto* sessions = m_audio->getSessionsForExecutable("Player");
        auto edit = [&](const QString& value) {
            if (policy == "commApps")
                return value.isEmpty() ? m_audio->removeCommApp("Player") : m_audio->addCommApp("Player");
            if (policy == "appRenames")
                return m_audio->setCustomApplicationName("Player", 0, value);
            if (policy == "executableRenames")
                return m_audio->setCustomExecutableName("Player", value);
            if (policy == "appLocks")
                return m_audio->setApplicationLocked("Player", 0, !value.isEmpty());
            if (policy == "deviceRenames")
                return m_audio->setCustomDeviceName("Player", value);
            if (policy == "deviceIcons")
                return m_audio->setCustomDeviceIcon("Player", value);
            return m_audio->setApplicationMutedInBackground("Player", !value.isEmpty());
        };
        auto state = [&]() -> QVariant {
            if (policy == "commApps") return m_audio->isCommApp("Player");
            if (policy == "appRenames") return m_audio->getCustomApplicationName("Player", 0);
            if (policy == "executableRenames") return m_audio->getCustomExecutableName("Player");
            if (policy == "appLocks") return m_audio->isApplicationLocked("Player", 0);
            if (policy == "deviceRenames") return m_audio->getCustomDeviceName("Player");
            if (policy == "deviceIcons") return m_audio->getCustomDeviceIcon("Player");
            return m_audio->isApplicationMutedInBackground("Player");
        };
        if (operation != "add")
            QVERIFY(edit("headset"));
        const auto previous = state();
        const auto saved = JsonStore::load(path, policy);
        QVERIFY(!saved.isNull());
        QSignalSpy failed(m_audio.get(), &AudioBridge::saveFailed);
        QSignalSpy commChanged(m_audio.get(), &AudioBridge::commAppsListChanged);
        QSignalSpy lockChanged(m_audio.get(), &AudioBridge::applicationLockChanged);
        QSignalSpy renameChanged(m_audio.get(), &AudioBridge::deviceRenameUpdated);
        QSignalSpy iconChanged(m_audio.get(), &AudioBridge::deviceIconUpdated);
        QSignalSpy sessionChanged(sessions, &QAbstractItemModel::dataChanged);
        QSignalSpy groupChanged(m_audio->groupedApplications(), &QAbstractItemModel::dataChanged);
        QSignalSpy deviceChanged(m_audio->outputDevices(), &QAbstractItemModel::dataChanged);
        const QString next = operation == "remove" ? QString{} : QString("speaker");
        {
            const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            QVERIFY(!edit(next));
            QCOMPARE(state(), previous);
            QCOMPARE(JsonStore::load(path, policy), saved);
            QCOMPARE(failed.size(), 1);
            QCOMPARE(failed.first().first().toString(), m_audio->lastError());
            QVERIFY(!m_audio->lastError().isEmpty());
            QCOMPARE(commChanged.size() + lockChanged.size() + renameChanged.size() + iconChanged.size()
                     + sessionChanged.size() + groupChanged.size() + deviceChanged.size(), 0);
        }
        QVERIFY(edit(next));
        QVERIFY(m_audio->lastError().isEmpty());
        const auto accepted = state();
        QVERIFY(accepted != previous);
        const auto acceptedFile = JsonStore::load(path, policy);
        QVERIFY(acceptedFile != saved);
        QVERIFY(edit(next)); // Repeated edits must not append duplicate rules.
        QCOMPARE(JsonStore::load(path, policy), acceptedFile);
        m_audio.reset();
        m_audio.reset(AudioBridge::instance());
        QCOMPARE(state(), accepted);
    }
    void firstAudioPolicySaveFailure()
    {
        m_shortcuts.reset(KeyboardShortcutManager::instance());
        m_audio.reset(AudioBridge::instance());
        const QString path = m_shortcutDataDirectory + "/commapps.json";
        QVERIFY(QDir().mkpath(path));
        QVERIFY(!m_audio->addCommApp("Player"));
        QVERIFY(!m_audio->isCommApp("Player"));
        QVERIFY(QDir().rmdir(path));
        QVERIFY(m_audio->addCommApp("Player"));
        QVERIFY(m_audio->isCommApp("Player"));
    }
    void rememberedApplicationVolumesPersistAndIgnorePolicyWrites()
    {
        auto* settings = UserSettings::instance();
        settings->setRememberApplicationVolumes(true);
        m_audio.reset(AudioBridge::instance());
        AudioApplication player;
        player.id = "player-session";
        player.name = "Player";
        player.executableName = "Player";
        player.volume = 30;
        AudioApplication systemSounds;
        systemSounds.id = "system_sounds";
        systemSounds.name = "System sounds";
        systemSounds.executableName = "System sounds";
        systemSounds.volume = 80;
        systemSounds.isSystemSounds = true;
        AudioManager::instance()->applicationsChanged({player, systemSounds});

        AudioManager::instance()->applicationVolumeChanged(player.id, 42, false);
        AudioManager::instance()->applicationVolumeChanged(systemSounds.id, 15, false);
        const QString path = m_shortcutDataDirectory + "/applicationvolumes.json";
        QTRY_VERIFY(!JsonStore::load(path, "applicationVolumes").isNull());
        QCOMPARE(m_audio->rememberedApplicationVolume("PLAYER"), 42);
        QCOMPARE(m_audio->rememberedApplicationVolume("System sounds"), -1);

        AudioManager::instance()->applicationVolumeChanged(player.id, 10, true);
        m_audio->applyChatMixToApplications(50);
        QTest::qWait(350);
        QCOMPARE(m_audio->rememberedApplicationVolume("player"), 42);
        const QJsonDocument saved = JsonStore::load(path, "applicationVolumes");
        const QJsonArray entries = saved.object().value("applicationVolumes").toArray();
        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries.first().toObject().value("executableName").toString(), QString("player"));
        QCOMPARE(entries.first().toObject().value("volume").toInt(), 42);

        m_audio.reset();
        m_audio.reset(AudioBridge::instance());
        QCOMPARE(m_audio->rememberedApplicationVolume("Player"), 42);
        QVERIFY(m_audio->clearRememberedApplicationVolumes());
        QCOMPARE(m_audio->rememberedApplicationVolume("Player"), -1);
        QCOMPARE(JsonStore::load(path, "applicationVolumes").object()
                     .value("applicationVolumes").toArray().size(), 0);
    }
    void rememberedApplicationVolumesClearRequiresDurableSave()
    {
        const QString path = m_shortcutDataDirectory + "/applicationvolumes.json";
        const QJsonDocument saved(QJsonObject{{"applicationVolumes", QJsonArray{
            QJsonObject{{"executableName", "player"}, {"volume", 37}}}}});
        QVERIFY(JsonStore::save(path, saved));
        m_audio.reset(AudioBridge::instance());
        QCOMPARE(m_audio->rememberedApplicationVolume("Player"), 37);
        QSignalSpy failed(m_audio.get(), &AudioBridge::saveFailed);
        {
            const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            QVERIFY(!m_audio->clearRememberedApplicationVolumes());
            QCOMPARE(m_audio->rememberedApplicationVolume("Player"), 37);
            QCOMPARE(JsonStore::load(path, "applicationVolumes"), saved);
            QCOMPARE(failed.size(), 1);
        }
        QVERIFY(m_audio->clearRememberedApplicationVolumes());
        QCOMPARE(m_audio->rememberedApplicationVolume("Player"), -1);
    }
#endif
#ifdef QONTROLPANEL_LANGUAGE_TESTS
    void languageChangesRequireSavedSettings()
    {
        auto* settings = UserSettings::instance();
        const int originalLanguage = settings->languageIndex();
        const auto restoreLanguage = qScopeGuard([&] { settings->setLanguageIndex(originalLanguage); });
        std::unique_ptr<Updater> updater(Updater::instance());
        std::unique_ptr<LanguageBridge> language(LanguageBridge::instance());
        int german = -1;
        int english = -1;
        for (int i = 1; i <= language->getLanguageNativeNames().size(); ++i) {
            if (language->getLanguageCodeFromIndex(i) == "de") german = i;
            if (language->getLanguageCodeFromIndex(i) == "en") english = i;
        }
        QVERIFY(german > 0 && english > 0);
        settings->setLanguageIndex(german);
        language->reloadApplicationLanguage(); // The startup path also uses the accepted setting.
        QCOMPARE(QCoreApplication::translate("LanguagePane", "Language"), QString("Sprache"));
        QSignalSpy changed(language.get(), &LanguageBridge::languageChanged);
        QSignalSpy settingChanged(settings, &UserSettings::languageIndexChanged);
        QSignalSpy failed(settings, &UserSettings::saveFailed);
        QFile preferences(m_preferencesPath);
        QVERIFY(preferences.open(QIODevice::ReadOnly));
        const auto saved = preferences.readAll();
        preferences.close();
        {
            const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(m_preferencesPath.utf16()), GENERIC_READ,
                                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            settings->setLanguageIndex(english);
            QCOMPARE(settings->languageIndex(), german);
            QCOMPARE(settingChanged.size(), 0);
            QCOMPARE(changed.size(), 0);
            QCOMPARE(failed.size(), 1);
            QCOMPARE(QCoreApplication::translate("LanguagePane", "Language"), QString("Sprache"));
            QVERIFY(preferences.open(QIODevice::ReadOnly));
            QCOMPARE(preferences.readAll(), saved);
            preferences.close();

            // A completed download must reload the persisted language, even after a rejected edit.
            updater->translationDownloadFinished(true, QString{});
            QCOMPARE(changed.size(), 1);
            QCOMPARE(QCoreApplication::translate("LanguagePane", "Language"), QString("Sprache"));
        }
        settings->setLanguageIndex(english);
        QCOMPARE(settings->languageIndex(), english);
        QCOMPARE(settingChanged.size(), 1);
        QCOMPARE(changed.size(), 2);
        QCOMPARE(QCoreApplication::translate("LanguagePane", "Language"), QString("Language"));
        QVERIFY(settings->lastError().isEmpty());
        QSettings persisted(QSettings::IniFormat, QSettings::UserScope, "ChrisLauinger77", "QontrolPanel");
        QCOMPARE(persisted.value("languageIndex").toInt(), english);
    }
#endif
    void globalShortcutChangesRequireSavedSettings()
    {
        auto* settings = UserSettings::instance();
        settings->setGlobalShortcutsEnabled(true);
        constexpr int modifiers = Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier;
        m_shortcuts.reset(KeyboardShortcutManager::instance());
        QVERIFY2(m_shortcuts->lastError().isEmpty(), qPrintable(m_shortcuts->lastError()));
        QVERIFY(!shortcutIsAvailable(VK_F17));
        QVERIFY(shortcutIsAvailable(VK_F22));
        QVERIFY(m_shortcuts->addAppVolumeHotkey("player.exe", Qt::Key_F20, modifiers, Qt::Key_F21, modifiers));
        QVERIFY(!shortcutIsAvailable(VK_F20));
        QFile preferences(m_preferencesPath);
        QVERIFY(preferences.open(QIODevice::ReadOnly));
        const auto saved = preferences.readAll();
        preferences.close();
        QSignalSpy changed(settings, &UserSettings::globalShortcutsEnabledChanged);
        QSignalSpy failed(settings, &UserSettings::saveFailed);
        {
            const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(m_preferencesPath.utf16()), GENERIC_READ,
                                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            settings->setGlobalShortcutsEnabled(false);
            QCoreApplication::processEvents();
            QVERIFY(settings->globalShortcutsEnabled());
            QCOMPARE(changed.size(), 0);
            QCOMPARE(failed.size(), 1);
            QVERIFY(!shortcutIsAvailable(VK_F20));
            QVERIFY(!shortcutIsAvailable(VK_F21));
            QVERIFY(preferences.open(QIODevice::ReadOnly));
            QCOMPARE(preferences.readAll(), saved);
            preferences.close();
        }
        settings->setGlobalShortcutsEnabled(false);
        QVERIFY(!settings->globalShortcutsEnabled());
        QCOMPARE(changed.size(), 1);
        QVERIFY(shortcutIsAvailable(VK_F20));
        QVERIFY(shortcutIsAvailable(VK_F21));
        settings->setGlobalShortcutsEnabled(true);
        QTRY_VERIFY(!shortcutIsAvailable(VK_F20));
        QVERIFY(!shortcutIsAvailable(VK_F21));
        QCOMPARE(changed.size(), 2);

        // Accepted key/modifier edits also apply without a settings pane calling the manager.
        const int oldKey = settings->panelShortcutKey();
        const int oldModifiers = settings->panelShortcutModifiers();
        settings->setPanelShortcutModifiers(modifiers);
        settings->setPanelShortcutKey(Qt::Key_F22);
        QTRY_VERIFY(!shortcutIsAvailable(VK_F22));
        settings->setPanelShortcutKey(oldKey);
        settings->setPanelShortcutModifiers(oldModifiers);
        QTRY_VERIFY(shortcutIsAvailable(VK_F22));
        QVERIFY(!shortcutIsAvailable(VK_F17));
        QCOMPARE(settings->panelShortcutKey(), oldKey);
        QCOMPARE(settings->panelShortcutModifiers(), oldModifiers);
    }
    void shortcutSaveFailure_data()
    {
        QTest::addColumn<QString>("operation");
        QTest::newRow("add") << QString("add");
        QTest::newRow("replace") << QString("replace");
        QTest::newRow("remove") << QString("remove");
    }
    void shortcutSaveFailure()
    {
        QFETCH(QString, operation);
        constexpr int modifiers = Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier;
        QVERIFY(shortcutIsAvailable(VK_F20));
        QVERIFY(shortcutIsAvailable(VK_F21));
        QVERIFY(shortcutIsAvailable(VK_F22));
        QVERIFY(shortcutIsAvailable(VK_F23));
        m_shortcuts.reset(KeyboardShortcutManager::instance());
        QVERIFY(m_shortcuts->addAppVolumeHotkey("player.exe", Qt::Key_F20, modifiers, Qt::Key_F21, modifiers, 2));
        const auto previous = m_shortcuts->appVolumeHotkeysJson();
        const QString path = m_shortcutDataDirectory + "/appvolumehotkeys.json";
        const auto saved = JsonStore::load(path);
        QCOMPARE(saved, QJsonDocument(previous));
        const bool enabled = UserSettings::instance()->globalShortcutsEnabled();
        QCOMPARE(shortcutIsAvailable(VK_F20), !enabled);
        QCOMPARE(shortcutIsAvailable(VK_F21), !enabled);

        QSignalSpy changed(m_shortcuts.get(), &KeyboardShortcutManager::appVolumeHotkeysChanged);
        QSignalSpy failed(m_shortcuts.get(), &KeyboardShortcutManager::saveFailed);
        QSignalSpy errorChanged(m_shortcuts.get(), &KeyboardShortcutManager::lastErrorChanged);
        auto edit = [&] {
            if (operation == "remove")
                return m_shortcuts->removeAppVolumeHotkey("player.exe");
            return m_shortcuts->addAppVolumeHotkey(operation == "replace" ? "player.exe" : "other.exe",
                                                  Qt::Key_F22, modifiers, Qt::Key_F23, modifiers, 5);
        };
        {
            // Permit reads, but block the atomic replacement of the existing JSON file.
            const HANDLE lock = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ,
                                            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            QVERIFY(lock != INVALID_HANDLE_VALUE);
            const auto unlock = qScopeGuard([&] { CloseHandle(lock); });
            QVERIFY(!edit());
            QCOMPARE(m_shortcuts->appVolumeHotkeysJson(), previous);
            QCOMPARE(JsonStore::load(path), saved);
            QCOMPARE(changed.size(), 0);
            QCOMPARE(failed.size(), 1);
            QVERIFY(!m_shortcuts->lastError().isEmpty());
            QCOMPARE(failed.first().first().toString(), m_shortcuts->lastError());
            QCOMPARE(errorChanged.size(), 1);
            QCOMPARE(shortcutIsAvailable(VK_F20), !enabled);
            QCOMPARE(shortcutIsAvailable(VK_F21), !enabled);
            QVERIFY(shortcutIsAvailable(VK_F22));
            QVERIFY(shortcutIsAvailable(VK_F23));
        }
        QVERIFY(edit());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(failed.size(), 1);
        QVERIFY(m_shortcuts->lastError().isEmpty());
        QCOMPARE(errorChanged.size(), 2);
        const auto expected = m_shortcuts->appVolumeHotkeysJson();
        QCOMPARE(expected.size(), operation == "add" ? 2 : operation == "replace" ? 1 : 0);
        QCOMPARE(JsonStore::load(path), QJsonDocument(expected));
        QCOMPARE(shortcutIsAvailable(VK_F20), !(enabled && operation == "add"));
        QCOMPARE(shortcutIsAvailable(VK_F21), !(enabled && operation == "add"));
        QCOMPARE(shortcutIsAvailable(VK_F22), !(enabled && operation != "remove"));
        QCOMPARE(shortcutIsAvailable(VK_F23), !(enabled && operation != "remove"));
        m_shortcuts.reset();
        m_shortcuts.reset(KeyboardShortcutManager::instance());
        QCOMPARE(m_shortcuts->appVolumeHotkeysJson(), expected);
    }
    void firstShortcutSaveFailure()
    {
        m_shortcuts.reset(KeyboardShortcutManager::instance());
        const QString path = m_shortcutDataDirectory + "/appvolumehotkeys.json";
        QVERIFY(QDir().mkpath(path)); // A directory at the file path prevents initial creation.
        QSignalSpy changed(m_shortcuts.get(), &KeyboardShortcutManager::appVolumeHotkeysChanged);
        QSignalSpy failed(m_shortcuts.get(), &KeyboardShortcutManager::saveFailed);
        constexpr int modifiers = Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier;
        QVERIFY(!m_shortcuts->addAppVolumeHotkey("player.exe", Qt::Key_F20, modifiers, Qt::Key_F21, modifiers));
        QVERIFY(m_shortcuts->appVolumeHotkeysJson().isEmpty());
        QCOMPARE(changed.size(), 0);
        QCOMPARE(failed.size(), 1);
        QVERIFY(shortcutIsAvailable(VK_F20));
        QVERIFY(shortcutIsAvailable(VK_F21));
        QVERIFY(QDir().rmdir(path));
        QVERIFY(m_shortcuts->addAppVolumeHotkey("player.exe", Qt::Key_F20, modifiers, Qt::Key_F21, modifiers));
        QCOMPARE(changed.size(), 1);
        QVERIFY(m_shortcuts->lastError().isEmpty());
    }
#endif
    void survivingStreamsKeepTheirRuleIndex()
    {
        SessionIndices indices;
        indices.update({{"player", "one"}, {"player", "two"}});
        QCOMPARE(indices.index("player", "one"), 0);
        QCOMPARE(indices.index("player", "two"), 1);
        indices.update({{"player", "two"}, {"player", "one"}});
        QCOMPARE(indices.index("player", "two"), 1);
        indices.update({{"player", "two"}});
        QCOMPARE(indices.index("player", "two"), 1);
        QCOMPARE(indices.index("player", "one"), -1);
        indices.update({{"player", "three"}, {"player", "two"}});
        QCOMPARE(indices.index("player", "three"), 0);
        QCOMPARE(indices.index("player", "two"), 1);
        indices.update({});
        QCOMPARE(indices.index("player", "two"), -1);
    }
    void modelUpdatesKeepIdentity()
    {
        ApplicationModel applications;
        ExecutableSessionModel sessions;
        FilteredDeviceModel devices(false);
        QAbstractItemModelTester applicationTester(&applications,
                                                   QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester sessionTester(&sessions, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QAbstractItemModelTester deviceTester(&devices, QAbstractItemModelTester::FailureReportingMode::QtTest);
        AudioApplication app;
        app.id = "instance";
        app.name = "Player";
        app.executableName = "player";
        applications.setApplications({app});
        sessions.setSessions({app});
        QSignalSpy resets(&applications, &QAbstractItemModel::modelReset);
        QPersistentModelIndex persistent(applications.index(0, 0));
        app.volume = 42;
        applications.setApplications({app});
        sessions.setSessions({app});
        QVERIFY(persistent.isValid());
        QCOMPARE(resets.size(), 0);
        QCOMPARE(applications.data(persistent, ApplicationModel::VolumeRole).toInt(), 42);
        AudioDevice device;
        device.id = "endpoint";
        device.isDefault = true;
        devices.setDevices({device});
        QCOMPARE(devices.rowCount(), 1);
        QCOMPARE(devices.getCurrentDefaultIndex(), 0);
        applications.setApplications({});
        sessions.setSessions({});
        devices.setDevices({});
        QVERIFY(!persistent.isValid());
        QCOMPARE(devices.getCurrentDefaultIndex(), -1);
    }
    void concurrentLogsAreBounded()
    {
        auto* logger = LogManager::instance();
        logger->clearLogs();
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i)
            threads.emplace_back([logger, i] {
                for (int n = 0; n < 1000; ++n)
                    logger->log(QString::number(i), QString::number(n));
            });
        for (auto& thread : threads)
            thread.join();
        QCOMPARE(logger->snapshot().size(), 500);
        for (int i = 0; i < 4; ++i)
            QVERIFY(logger->getAllCategories().contains(QString::number(i)));
        QSignalSpy updates(logger, &LogManager::bufferedLogsReady);
        logger->setQmlReady();
        for (int n = 0; n < 1000; ++n)
            logger->log("0", "entry");
        QCoreApplication::processEvents();
        QCOMPARE(updates.size(), 2); // initial snapshot plus one coalesced delivery
        QCOMPARE(logger->snapshot().size(), 500);
    }
    void corruptSettingsArePreserved()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("settings.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{broken");
        file.close();
        QVERIFY(JsonStore::load(path, "commApps").isNull());
        const QJsonDocument desired(
            QJsonObject{{"commApps", QJsonArray{QJsonObject{{"name", "Discord"}, {"icon", ""}}}}});
        QVERIFY(JsonStore::save(path, desired));
        QCOMPARE(JsonStore::load(path, "commApps"), desired);
        const auto backups = QDir(dir.path()).entryList({"settings.json.corrupt-*"}, QDir::Files);
        QCOMPARE(backups.size(), 1);
        QFile backup(dir.filePath(backups.first()));
        QVERIFY(backup.open(QIODevice::ReadOnly));
        QCOMPARE(backup.readAll(), QByteArray("{broken"));
        QVERIFY(!JsonStore::save(dir.path(), desired));
        QCOMPARE(JsonStore::load(path, "commApps"), desired);
    }
    void malformedSchemaIsRejected()
    {
        QVERIFY(!JsonStore::validate(QJsonDocument(QJsonObject{{"commApps", "wrong"}}), "commApps"));
        QVERIFY(!JsonStore::validate(QJsonDocument(QJsonObject{{"backgroundMutedApps", QJsonArray{17}}}),
                                     "backgroundMutedApps"));
        QVERIFY(!JsonStore::validate(QJsonDocument(QJsonObject{{"applicationVolumes", QJsonArray{
            QJsonObject{{"executableName", "Player"}, {"volume", 101}}}}}), "applicationVolumes"));
        QVERIFY(!JsonStore::validate(QJsonDocument(QJsonObject{{"applicationVolumes", QJsonArray{
            QJsonObject{{"executableName", ""}, {"volume", 50}}}}}), "applicationVolumes"));
    }
    void expiredUpdateStagingIsRemoved()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QDateTime now = QDateTime::currentDateTimeUtc();
        const QDir root(temporaryDirectory.path());
        const QStringList names = {"QontrolPanel-update-ABC001", "QontrolPanel-update-ABC002",
            "QontrolPanel-update-ABC003", "QontrolPanel-update-unrelated"};
        for (const auto& name : names) {
            QVERIFY(root.mkdir(name));
            QFile installer(root.filePath(name + "/QontrolPanel_Installer.exe"));
            QVERIFY(installer.open(QIODevice::WriteOnly));
            QCOMPARE(installer.write("installer"), 9);
            QVERIFY(installer.flush());
            QVERIFY(installer.setFileTime(now.addDays(-2), QFileDevice::FileModificationTime));
        }
        QFile recent(root.filePath(names[1] + "/QontrolPanel_Installer.exe"));
        QVERIFY(recent.open(QIODevice::ReadWrite));
        QVERIFY(recent.setFileTime(now, QFileDevice::FileModificationTime));
        recent.close();
        QFile unexpected(root.filePath(names[2] + "/keep.txt"));
        QVERIFY(unexpected.open(QIODevice::WriteOnly));
        unexpected.close();

        UpdateStaging::cleanup(root.path(), now);
        QVERIFY(!root.exists(names[0]));
        for (const auto& name : names.mid(1))
            QVERIFY(root.exists(name + "/QontrolPanel_Installer.exe"));
        QVERIFY(unexpected.exists());

        // A later launch cleans up the previously recent installer.
        UpdateStaging::cleanup(root.path(), now.addDays(2));
        QVERIFY(!root.exists(names[1]));
        QVERIFY(root.exists(names[2] + "/QontrolPanel_Installer.exe"));
        QVERIFY(root.exists(names[3] + "/QontrolPanel_Installer.exe"));
    }
#ifdef Q_OS_WIN
    void runningUpdateInstallerIsRetried()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        QTemporaryDir stagingDirectory(temporaryDirectory.filePath(UpdateStaging::DirectoryTemplate));
        QVERIFY(stagingDirectory.isValid());
        const QString installerPath = stagingDirectory.filePath(UpdateStaging::InstallerName);
        QFile installer(installerPath);
        QVERIFY(installer.open(QIODevice::WriteOnly));
        QCOMPARE(installer.write("installer"), 9);
        installer.close();
        const HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(installerPath.utf16()), GENERIC_READ,
            FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        QVERIFY(handle != INVALID_HANDLE_VALUE);
        auto releaseHandle = qScopeGuard([handle] { CloseHandle(handle); });
        const QDateTime later = QDateTime::currentDateTimeUtc().addDays(2);
        UpdateStaging::cleanup(temporaryDirectory.path(), later);
        QVERIFY(QFile::exists(installerPath));

        CloseHandle(handle);
        releaseHandle.dismiss();
        UpdateStaging::cleanup(temporaryDirectory.path(), later);
        QVERIFY(!QFile::exists(installerPath));
        QVERIFY(!QDir(stagingDirectory.path()).exists());
    }
#endif
    void reentrantCancellation()
    {
        QObject receiver;
        QList<QNetworkReply*> replies;
        int aborted = 0, completed = 0;
        for (int i = 0; i < 10; ++i)
        {
            auto* reply = new SynchronousReply();
            reply->aborted = &aborted;
            replies.append(reply);
            connect(reply, &QNetworkReply::finished, &receiver, [&, reply] {
                ++completed;
                replies.removeAll(reply);
            });
        }
        cancelReplyBatch(replies, &receiver);
        QCOMPARE(aborted, 10);
        QCOMPARE(completed, 0);
        QVERIFY(replies.isEmpty());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void cleanupPrecedesThreadExit()
    {
        std::atomic<int> operations{0};
        std::atomic<bool> cleaned{false}, destroyed{false};
        auto* worker = new FakeWorker;
        worker->operations = &operations;
        worker->cleaned = &cleaned;
        worker->destroyed = &destroyed;
        auto* thread = new QThread;
        worker->moveToThread(thread);
        thread->start();
        for (int i = 0; i < 20; ++i)
            QMetaObject::invokeMethod(worker, [&operations] { ++operations; }, Qt::QueuedConnection);
        retireWorkerThread(thread, worker, "cleanup");
        drainRetiredWorkerThreads();
        QCOMPARE(operations.load(), 20);
        QVERIFY(cleaned.load());
        QVERIFY(destroyed.load());
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) {});
    ReliabilityTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "reliability_tests.moc"
