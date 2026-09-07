#pragma once
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace UpdateStaging {
inline constexpr auto DirectoryTemplate = "QontrolPanel-update-XXXXXX";
inline constexpr auto InstallerName = "QontrolPanel_Installer.exe";

// Run on a background thread after acquiring the application's instance lock.
inline void cleanup(const QString& temporaryPath, const QDateTime& now)
{
    const QDir temporaryDirectory(temporaryPath);
    const QDateTime cutoff = now.addDays(-1);
    const QRegularExpression stagingName("^QontrolPanel-update-[A-Za-z0-9]{6}$");
    const auto directories = temporaryDirectory.entryInfoList(
        {"QontrolPanel-update-*"}, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const auto& directory : directories) {
        if (!stagingName.match(directory.fileName()).hasMatch() || directory.isJunction())
            continue;

        const QDir stagingDirectory(directory.absoluteFilePath());
        const auto entries = stagingDirectory.entryInfoList(
            QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        if (entries.size() == 1) {
            const auto& installer = entries.first();
            // Leave recent downloads, incomplete staging, links and unknown contents alone.
            if (installer.fileName() != InstallerName || !installer.isFile() || installer.isSymLink()
                || !installer.lastModified().isValid() || installer.lastModified() >= cutoff)
                continue;
            // Windows rejects removal while the detached installer still uses the executable.
            // Keep failed removals for a later launch, without deleting anything recursively.
            if (!QFile::remove(installer.absoluteFilePath()))
                continue;
        } else if (!entries.isEmpty() || !directory.lastModified().isValid()
                   || directory.lastModified() >= cutoff) {
            continue;
        }
        temporaryDirectory.rmdir(directory.fileName());
    }
}
}
