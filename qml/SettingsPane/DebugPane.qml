pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

ColumnLayout {
    id: logViewer
    spacing: 3

    property string selectedSender: LogBridge.allFilterValue
    property bool autoScroll: true

    Component.onCompleted: {
        LogBridge.applyFilter(selectedSender);
        updateCategoryList();
    }

    Connections {
        target: LogManager
        function onCategoryRegistered(category) {
            senderOptions.append({
                text: category,
                value: category
            });
        }
    }

    function updateCategoryList() {
        let categories = LogManager.getAllCategories();
        for (let i = 0; i < categories.length; i++) {
            senderOptions.append({
                text: categories[i],
                value: categories[i]
            });
        }
    }

    function getFilterLabel(filterValue) {
        return filterValue === LogBridge.allFilterValue ? qsTr("All") : filterValue;
    }

    function getSenderColor(sender) {
        const colorMap = {
            "AudioManager": "#ff1493",
            "MediaSessionManager": "#00bfff",
            "MonitorManager": "#32cd32",
            "SoundPanelBridge": "#ff6600",
            "Updater": "#9932cc",
            "ShortcutManager": "#1e90ff",
            "Core": "#dc143c",
            "LocalServer": "#8a2be2",
            "UI": "#ffd700",
            "PowerManager": "#ff69b4",
            "HeadsetControlManager": "#7fff00",
            "WindowFocusManager": "#00ff7f"
        };

        if (colorMap[sender]) {
            return colorMap[sender];
        }

        let hash = 0;
        for (let i = 0; i < sender.length; i++) {
            hash = sender.charCodeAt(i) + ((hash << 5) - hash);
        }
        const hue = Math.abs(hash % 360);
        return `hsl(${hue}, 70%, 60%)`;
    }

    function copyAllLogs() {
        let version = Updater.getAppVersion();
        let commitHash = Updater.getCommitHash();
        let currentDate = new Date().toISOString().split('T')[0];
        let header = qsTr("QontrolPanel Log Export") + "\n";
        header += "========================\n";
        header += "Version: " + version + "\n";
        header += "Commit: " + commitHash + "\n";
        header += "Export Date: " + currentDate + "\n";
        header += "Filter: " + getFilterLabel(selectedSender) + "\n";
        header += "Total Entries: " + LogBridge.filteredModel.count + "\n";
        header += "========================\n\n";

        let allLogsText = header;
        for (let i = 0; i < LogBridge.filteredModel.count; i++) {
            let item = LogBridge.filteredModel.get(i);
            allLogsText += item.message + "\n";
        }

        if (LogBridge.filteredModel.count > 0) {
            allLogsText = allLogsText.slice(0, -1);
        }

        hiddenTextEdit.text = allLogsText;
        hiddenTextEdit.selectAll();
        hiddenTextEdit.copy();
    }

    function scrollToBottom() {
        if (LogBridge.filteredModel.count > 0) {
            logListView.positionViewAtEnd();
        }
    }

    Connections {
        target: LogBridge

        function onLogEntryAdded(message, type, sender) {
            if (logViewer.autoScroll) {
                Qt.callLater(logViewer.scrollToBottom);
            }
        }
    }

    Label {
        text: qsTr("Console output")
        font.pixelSize: 22
        font.bold: true
        Layout.bottomMargin: 15
    }

    RowLayout {
        spacing: 10

        Label {
            text: qsTr("Filter by:")
        }

        CustomComboBox {
            id: senderFilter
            Layout.preferredWidth: 200
            model: ListModel {
                id: senderOptions
                ListElement {
                    text: qsTr("All")
                    value: "__ALL__"
                }
            }
            textRole: "text"
            valueRole: "value"
            onCurrentValueChanged: {
                logViewer.selectedSender = currentValue;
                LogBridge.applyFilter(logViewer.selectedSender);
            }
        }

        Item {
            Layout.fillWidth: true
        }

        CheckBox {
            text: qsTr("Auto-scroll")
            checked: logViewer.autoScroll
            onCheckedChanged: logViewer.autoScroll = checked
        }

        Button {
            text: qsTr("Copy All")
            onClicked: logViewer.copyAllLogs()
        }

        Button {
            text: qsTr("Clear")
            onClicked: LogBridge.clearLogs()
        }
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true

        CustomScrollView {
            anchors.fill: parent
            clip: true
            background: Rectangle {
                color: palette.window
                border.color: Constants.darkMode ? "#3e3e3e" : palette.mid
                border.width: 1
                radius: 4
            }

            ListView {
                id: logListView
                model: LogBridge.filteredModel
                clip: true
                delegate: Label {
                    required property var model
                    width: logListView.width
                    wrapMode: Text.Wrap
                    font.family: "Consolas, Monaco, Courier New, monospace"
                    font.pixelSize: 12
                    textFormat: Text.RichText
                    text: {
                        let typeColor = "";
                        switch (model.type) {
                        case LogManager.Info:
                            typeColor = "#00ff00";
                            break;
                        case LogManager.Warning:
                            typeColor = "#ff8800";
                            break;
                        case LogManager.Critical:
                            typeColor = "#ff2222";
                            break;
                        default:
                            typeColor = palette.text;
                        }
                        let senderColor = logViewer.getSenderColor(model.sender);
                        let message = model.message || "";
                        let regex = /^(\[[^\]]+\]\s+)(\w+)(\s+)\[(\w+)\](\s+-.*)$/;
                        let match = message.match(regex);
                        if (match) {
                            let timestamp = match[1];
                            let sender = match[2];
                            let spacer1 = match[3];
                            let type = match[4];
                            let content = match[5];
                            return `<span style="color: #888888;">${timestamp}</span><span style="color: ${senderColor};">${sender}</span><span style="color: #888888;">${spacer1}</span><span style="color: ${typeColor};">[${type}]</span><span style="color: ${palette.text};">${content}</span>`;
                        } else {
                            return `<span style="color: ${typeColor};">${message}</span>`;
                        }
                    }
                    topPadding: 1
                    bottomPadding: 1
                    leftPadding: 5
                    rightPadding: 5
                }
            }
        }

        TextEdit {
            id: hiddenTextEdit
            visible: false
            Layout.preferredWidth: 0
            Layout.preferredHeight: 0
        }
    }
}
