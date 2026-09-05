#include <QtTest>
#include <QSignalSpy>
#include <QAbstractItemModelTester>
#include "audiomodels.h"
#include "sessionindices.h"
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
#ifdef Q_OS_WIN
#include "keyboardshortcutmanager.h"
#include "usersettings.h"
#ifdef QONTROLPANEL_AUDIO_POLICY_TESTS
#include "audiobridge.h"
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
    }
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
