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
private slots:
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
