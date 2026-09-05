#pragma once
#include <QObject>
#include <QThread>

// Called on the GUI thread. Cleanup runs on the owning worker thread before quit.
// A QThread is never destroyed while running; retirement is asynchronous.
void retireWorkerThread(QThread* thread, QObject* worker, const char* cleanupSlot);
bool drainRetiredWorkerThreads(int timeoutMs = 10000);
