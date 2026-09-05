pragma Singleton
import QtQuick
import ChrisLauinger77.QontrolPanel

Item {
    id: logBridge

    readonly property int maxLogEntries: 500
    property ListModel logModel: ListModel {}
    property ListModel filteredModel: ListModel {}
    readonly property string allFilterValue: "__ALL__"
    property string currentFilter: ""
    property var senderRegex: /^\[[^\]]+\]\s+(\w+)\s+\[\w+\]/

    signal logEntryAdded(string message, int type, string sender)
    signal logsCleared()

    Component.onCompleted: {
        LogManager.setQmlReady()
        applyFilter(allFilterValue)
    }

    Connections {
        target: LogManager

        function onBufferedLogsReady(logs) {
            logModel.clear()
            filteredModel.clear()
            for (let i = 0; i < logs.length; i++) {
                let log = logs[i]
                logBridge.addLogEntry(log.message, log.type)
            }
        }
    }

    function addLogEntry(message, type) {
        let sender = extractSenderFromMessage(message)

        // Proper cleanup - remove excess entries FIRST
        while (logModel.count >= maxLogEntries) {
            let oldest = logModel.get(0)
            if (currentFilter === allFilterValue || oldest.sender === currentFilter) {
                if (filteredModel.count > 0) filteredModel.remove(0)
            }
            logModel.remove(0)
        }

        let newEntry = {
            "message": message,
            "type": type,
            "sender": sender
        }

        logModel.append(newEntry)

        // Only add to filtered model if it matches current filter
        if (currentFilter === allFilterValue || sender === currentFilter) {
            filteredModel.append(newEntry)
        }

        logEntryAdded(message, type, sender)
    }

    function extractSenderFromMessage(message) {
        let match = message.match(senderRegex)
        return match ? match[1] : qsTr("Unknown")
    }

    function clearLogs() {
        LogManager.clearLogs()
        logsCleared()
    }

    function applyFilter(selectedSender) {
        if (selectedSender === currentFilter) return // No change needed

        currentFilter = selectedSender
        filteredModel.clear()

        if (selectedSender === allFilterValue) {
            // Copy all entries
            for (let i = 0; i < logModel.count; i++) {
                filteredModel.append(logModel.get(i))
            }
        } else {
            // Filter by sender
            for (let i = 0; i < logModel.count; i++) {
                let item = logModel.get(i)
                if (item.sender === selectedSender) {
                    filteredModel.append(item)
                }
            }
        }
    }
}
