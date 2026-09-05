#include "workerthreads.h"
#include "logmanager.h"
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QPointer>
#include <QVector>

namespace
{
    QVector<QPointer<QThread>>& retiringThreads()
    {
        static QVector<QPointer<QThread>> threads;
        return threads;
    }
} // namespace

void retireWorkerThread(QThread* thread, QObject* worker, const char* cleanupSlot)
{
    Q_ASSERT(QThread::currentThread() == QCoreApplication::instance()->thread());
    if (!thread)
        return;
    thread->setParent(nullptr);
    auto& threads = retiringThreads();
    threads.removeIf([](const auto& entry) { return entry.isNull(); });
    threads.append(thread);
    QObject::connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    QMetaObject::invokeMethod(
        worker,
        [thread, worker, cleanupSlot] {
            QMetaObject::invokeMethod(worker, cleanupSlot, Qt::DirectConnection);
            thread->quit();
        },
        Qt::QueuedConnection);
}

bool drainRetiredWorkerThreads(int timeoutMs)
{
    QDeadlineTimer deadline(timeoutMs);
    bool stopped = true;
    for (const auto& thread : retiringThreads())
    {
        if (!thread)
            continue;
        if (thread->wait(deadline))
        {
            delete thread.data();
        }
        else
        {
            stopped = false;
            // Native drivers can block beyond our deadline. Killing the thread
            // corrupts COM/HID state. Leave it owned by the exiting process.
            LOG_CRITICAL("Core", "Worker shutdown deadline exceeded; leaving native resources to process teardown");
        }
    }
    retiringThreads().clear();
    return stopped;
}
