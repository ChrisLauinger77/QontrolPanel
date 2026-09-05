#pragma once
#include <QString>
#include <QList>
#include <QMetaType>

struct AudioApplication
{
    QString id;             // Core Audio session instance identifier
    QString name;           // Display name
    QString executableName; // Executable name
    QString iconPath;       // Base64 encoded icon
    int volume;             // 0-100
    bool isMuted;           // Mute state
    int streamIndex;        // Index within the same executable (0, 1, 2, ...)
    bool isSystemSounds;    // Stable internal flag for Windows system sounds

    AudioApplication() : volume(0), isMuted(false), streamIndex(0), isSystemSounds(false) {}

    bool operator==(const AudioApplication& other) const { return id == other.id; }
};

struct AudioDevice
{
    QString id;
    QString name;
    QString description;
    QString shortName;
    bool isDefault;
    bool isDefaultCommunication;
    bool isInput;
    QString state;
    QString vendorId;      // USB VID
    QString productId;     // USB PID
    int batteryPercentage; // Battery level 0-100, -1 if not available
    QString batteryStatus; // "BATTERY_AVAILABLE", "BATTERY_CHARGING", "BATTERY_UNAVAILABLE"

    AudioDevice()
        : isDefault(false), isDefaultCommunication(false), isInput(false), batteryPercentage(-1),
          batteryStatus("BATTERY_UNAVAILABLE")
    {
    }

    bool operator==(const AudioDevice& other) const { return id == other.id; }
};

Q_DECLARE_METATYPE(AudioApplication)
Q_DECLARE_METATYPE(AudioDevice)
