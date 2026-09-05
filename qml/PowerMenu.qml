import QtQuick.Controls.FluentWinUI3
import ChrisLauinger77.QontrolPanel

Menu {
    enabled: !PowerBridge.busy
    id: powerMenu

    signal setPowerAction(int action)

    MenuItem {
        enabled: PowerBridge.sleepSupported
        icon.source: "qrc:/icons/sleep.svg"
        text: qsTr("Sleep")
        onTriggered: PowerBridge.sleep()
    }

    MenuItem {
        icon.source: "qrc:/icons/hibernate.svg"
        text: qsTr("Hibernate")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(0)
            } else {
                PowerBridge.hibernate()
            }
        }
        enabled: PowerBridge.hibernateSupported
    }

    MenuItem {
        icon.source: "qrc:/icons/restart.svg"
        text: qsTr("Restart")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(1)
            } else {
                PowerBridge.restart()
            }
        }
    }

    MenuItem {
        icon.source: "qrc:/icons/restart.svg"
        text: qsTr("Restart UEFI")
        enabled: PowerBridge.uefiSupported
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(4)
            } else {
                PowerBridge.restartToUEFI()
            }
        }
    }

    MenuItem {
        icon.source: "qrc:/icons/shutdown.svg"
        text: qsTr("Shutdown")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(2)
            } else {
                PowerBridge.shutdown()
            }
        }
    }

    MenuSeparator {}

    MenuItem {
        icon.source: "qrc:/icons/lock.svg"
        text: qsTr("Lock")
        onTriggered: PowerBridge.lockAccount()
    }

    MenuItem {
        icon.source: "qrc:/icons/signout.svg"
        text: qsTr("Sign Out")
        onTriggered: {
            if (UserSettings.showPowerDialogConfirmation) {
                powerMenu.setPowerAction(3)
            } else {
                PowerBridge.signOut()
            }
        }
    }

    MenuItem {
        icon.source: "qrc:/icons/switch.svg"
        enabled: PowerBridge.multipleUsers
        text: qsTr("Switch User")
        onTriggered: PowerBridge.switchAccount()
    }
}
