pragma Singleton
import QtQml

QtObject {
    function formatTime(status: string, timeToFullMinutes: int, timeToEmptyMinutes: int): string {
        const charging = status === "BATTERY_CHARGING"
        if (!charging && status !== "BATTERY_AVAILABLE")
            return ""
        const minutes = charging ? timeToFullMinutes : timeToEmptyMinutes
        if (minutes < 0)
            return ""
        if (minutes < 60) {
            return charging ? qsTr("about %1 min to full").arg(minutes)
                            : qsTr("about %1 min left").arg(minutes)
        }
        const hours = Math.round(minutes / 60)
        return charging ? qsTr("about %1 h to full").arg(hours)
                        : qsTr("about %1 h left").arg(hours)
    }
}
