#include "updater.h"
#include "replybatch.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QLocale>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrl>
#include <QTranslator>
#include <QVersionNumber>
#include <QRegularExpression>
#include <utility>
#include <QDebug>
#include <headsetcontrol.hpp>
#include <string>
#include "version.h"
#include "logmanager.h"
#include "languages.h"
#include "usersettings.h"

Updater* Updater::m_instance = nullptr;

namespace {
QString formatLocalizedTimestamp(const QString& timestamp)
{
    const QString trimmedTimestamp = timestamp.trimmed();
    if (trimmedTimestamp.isEmpty()) {
        return QString();
    }

    QDateTime parsedDateTime;
    QString normalizedTimestamp = trimmedTimestamp;

    const int separatorIndex = normalizedTimestamp.lastIndexOf(' ');
    if (separatorIndex > 0) {
        const QString timezonePart = normalizedTimestamp.mid(separatorIndex + 1);
        if ((timezonePart.startsWith('+') || timezonePart.startsWith('-'))
            && timezonePart.size() == 5
            && !timezonePart.contains(':')) {
            normalizedTimestamp.insert(separatorIndex + 4, ':');
        }
    }

    parsedDateTime = QDateTime::fromString(normalizedTimestamp, Qt::ISODate);
    if (!parsedDateTime.isValid()) {
        parsedDateTime = QDateTime::fromString(trimmedTimestamp, "yyyy-MM-dd HH:mm:ss 'UTC'");
        if (parsedDateTime.isValid()) {
            parsedDateTime.setTimeZone(QTimeZone::UTC);
        }
    }

    if (!parsedDateTime.isValid()) {
        parsedDateTime = QDateTime::fromString(trimmedTimestamp, "yyyy-MM-dd HH:mm:ss");
        if (parsedDateTime.isValid()) {
            parsedDateTime.setTimeSpec(Qt::LocalTime);
        }
    }

    if (!parsedDateTime.isValid()) {
        return trimmedTimestamp;
    }

    return QLocale::system().toString(parsedDateTime.toLocalTime(), QLocale::ShortFormat);
}
}

Updater* Updater::instance()
{
    if (!m_instance) {
        m_instance = new Updater();
    }
    return m_instance;
}

Updater* Updater::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

Updater::Updater(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_updateAvailable(false)
    , m_isChecking(false)
    , m_isDownloading(false)
    , m_downloadProgress(0)
    , m_totalTranslationDownloads(0)
    , m_completedTranslationDownloads(0)
    , m_failedTranslationDownloads(0)
    , m_translationAutoUpdateTimer(new QTimer(this))
    , m_appUpdateCheckTimer(new QTimer(this))
{
    m_networkManager->setTransferTimeout(30000);
    loadTranslationProgressData();

    m_translationAutoUpdateTimer->setInterval(4 * 60 * 60 * 1000);
    m_translationAutoUpdateTimer->setSingleShot(false);
    connect(m_translationAutoUpdateTimer, &QTimer::timeout, this, &Updater::checkForTranslationUpdates);
    m_translationAutoUpdateTimer->start();

    m_appUpdateCheckTimer->setInterval(4 * 60 * 60 * 1000);
    m_appUpdateCheckTimer->setSingleShot(false);
    connect(m_appUpdateCheckTimer, &QTimer::timeout, this, &Updater::checkForAppUpdatesTimer);
    m_appUpdateCheckTimer->start();

    if (UserSettings::instance()->autoUpdateTranslations()) {
        QTimer::singleShot(5000, this, [this]() {
            downloadLatestTranslations();
        });
    }

    if (UserSettings::instance()->autoFetchForAppUpdates()) {
        QTimer::singleShot(5000, this, [this]() {
            checkForAppUpdatesAuto();
        });
    }
}

void Updater::checkForUpdates()
{
    if (m_isChecking || m_isDownloading) {
        return;
    }

    LOG_INFO("Updater", "Checking for application updates");
    setChecking(true);

    m_updateAvailable = false;
    m_downloadUrl.clear();
    m_expectedSha256.clear();
    emit updateAvailableChanged();

    // Clear previous release notes
    setReleaseNotes("");

    QString urlString = "https://api.github.com/repos/ChrisLauinger77/QontrolPanel/releases/latest";
    QUrl url{urlString};
    QNetworkRequest request{url};
    request.setHeader(QNetworkRequest::UserAgentHeader, "QontrolPanel-Updater");

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > 2 * 1024 * 1024 || total > 2 * 1024 * 1024)
            reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, &Updater::onVersionCheckFinished);
}

void Updater::onVersionCheckFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    setChecking(false);

    if (reply->error() != QNetworkReply::NoError) {
        LOG_CRITICAL("Updater",
                     QString("Update check failed: %1").arg(reply->errorString()));
        emit updateFinished(false, tr("Failed to check for updates: %1").arg(reply->errorString()));
        return;
    }

    const auto data = reply->readAll();
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (data.size() > 2 * 1024 * 1024 || parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        emit updateFinished(false, tr("Invalid release metadata"));
        return;
    }
    const QJsonObject obj = doc.object();

    m_latestVersion = obj["tag_name"].toString();
    if (m_latestVersion.startsWith("v")) {
        m_latestVersion = m_latestVersion.mid(1);
    }

    if (!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(m_latestVersion).hasMatch())
    {
        emit updateFinished(false, tr("Invalid release metadata"));
        return;
    }
    // Extract release notes
    QString releaseBody = obj["body"].toString();
    if (!releaseBody.isEmpty()) {
        setReleaseNotes(releaseBody);
    }

    // Find the .exe asset
    QJsonArray assets = obj["assets"].toArray();
    m_downloadUrl.clear();

    for (const auto& asset : assets) {
        QJsonObject assetObj = asset.toObject();
        QString name = assetObj["name"].toString();
        if (name == "QontrolPanel_Installer.exe")
        {
            const QUrl assetUrl(assetObj["browser_download_url"].toString());
            if (assetUrl.scheme() != "https" || assetUrl.host() != "github.com" ||
                !assetUrl.path().startsWith("/ChrisLauinger77/QontrolPanel/releases/download/"))
                continue;
            m_downloadUrl = assetUrl.toString();
            const QString digest = assetObj["digest"].toString();
            if (QRegularExpression("^sha256:[0-9a-fA-F]{64}$").match(digest).hasMatch())
                m_expectedSha256 = QByteArray::fromHex(digest.mid(7).toLatin1());
            m_expectedSize = assetObj["size"].toInteger();
            break;
        }
    }

    if (m_downloadUrl.isEmpty()) {
        LOG_WARN("Updater", "Update check completed, but the latest release has no executable asset");
        emit updateFinished(false, tr("No executable found in latest release"));
        return;
    }

    QString currentVersion = getCurrentVersion();
    bool wasUpdateAvailable = m_updateAvailable;
    m_updateAvailable = isNewerVersion(m_latestVersion, currentVersion);

    if (wasUpdateAvailable != m_updateAvailable) {
        emit updateAvailableChanged();
    }
    emit latestVersionChanged();

    if (m_updateAvailable) {
        LOG_INFO("Updater",
                 QString("Application update available: %1").arg(m_latestVersion));
        emit updateFinished(true, tr("Update available: %1").arg(m_latestVersion));
    } else {
        LOG_INFO("Updater", "Application is using the latest version");
        emit updateFinished(true, tr("You are using the latest version"));
    }
}

void Updater::downloadAndInstall()
{
    if (m_downloadUrl.isEmpty() || m_isDownloading || m_isChecking)
        return;
    if (m_expectedSha256.size() != 32 || m_expectedSize <= 0 || m_expectedSize > 512 * 1024 * 1024)
    {
        emit updateFinished(false, tr("The release has no valid SHA-256 checksum or file size."));
        return;
    }
    m_installerFile.reset();
    m_stagingDirectory = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/QontrolPanel-update-XXXXXX");
    m_installerFile = std::make_unique<QSaveFile>(m_stagingDirectory->filePath("QontrolPanel_Installer.exe"));
    if (!m_stagingDirectory->isValid() || !m_installerFile->open(QIODevice::WriteOnly))
    {
        emit updateFinished(false, tr("Failed to save update file"));
        return;
    }
    m_receivedSize = 0;
    m_downloadHash.reset();
    m_downloadError.clear();
    setDownloading(true);
    setDownloadProgress(0);
    QNetworkRequest request{QUrl(m_downloadUrl)};
    auto* reply = m_networkManager->get(request);
    reply->setReadBufferSize(64 * 1024);
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] { writeDownloadChunk(reply); });
    connect(reply, &QNetworkReply::downloadProgress, this, &Updater::onDownloadProgress);
    connect(reply, &QNetworkReply::finished, this, &Updater::onDownloadFinished);
}

void Updater::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percentage = static_cast<int>((bytesReceived * 100) / bytesTotal);
        setDownloadProgress(percentage);
    }
}

void Updater::onDownloadFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    writeDownloadChunk(reply);
    reply->deleteLater();
    setDownloading(false);
    setDownloadProgress(0);
    if (reply->error() != QNetworkReply::NoError || !m_downloadError.isEmpty())
    {
        m_installerFile.reset();
        emit updateFinished(
            false, tr("Download failed: %1").arg(m_downloadError.isEmpty() ? reply->errorString() : m_downloadError));
        return;
    }
    if (m_receivedSize != m_expectedSize || m_downloadHash.result() != m_expectedSha256)
    {
        m_installerFile.reset();
        emit updateFinished(false, tr("Update verification failed. The installer was discarded."));
        return;
    }
    const QString path = m_installerFile->fileName();
    if (!m_installerFile->commit())
    {
        emit updateFinished(false, tr("Failed to save update file"));
        m_installerFile.reset();
        return;
    }
    m_installerFile.reset();
    installExecutable(path);
}

void Updater::installExecutable(const QString& newExePath)
{
    if (QProcess::startDetached(newExePath))
    {
        // Windows needs the installer after this process exits.
        m_stagingDirectory->setAutoRemove(false);
        emit updateFinished(true, tr("Update started."));
        QApplication::quit();
    }
    else
    {
        emit updateFinished(false, tr("Failed to start update executable"));
    }
}

QString Updater::getCurrentVersion() const
{
    return QString(APP_VERSION_STRING);
}

bool Updater::isNewerVersion(const QString& latest, const QString& current) const
{
    return QVersionNumber::fromString(latest) > QVersionNumber::fromString(current);
}

void Updater::setChecking(bool checking)
{
    if (m_isChecking != checking) {
        m_isChecking = checking;
        emit isCheckingChanged();
    }
}

void Updater::setDownloading(bool downloading)
{
    if (m_isDownloading != downloading) {
        m_isDownloading = downloading;
        emit isDownloadingChanged();
    }
}

void Updater::setDownloadProgress(int progress)
{
    if (m_downloadProgress != progress) {
        m_downloadProgress = progress;
        emit downloadProgressChanged();
    }
}

void Updater::setReleaseNotes(const QString& notes)
{
    if (m_releaseNotes != notes) {
        bool hadNotes = !m_releaseNotes.isEmpty();
        m_releaseNotes = notes;
        bool hasNotes = !m_releaseNotes.isEmpty();

        emit releaseNotesChanged();

        if (hadNotes != hasNotes) {
            emit hasReleaseNotesChanged();
        }
    }
}

void Updater::checkForTranslationUpdates()
{
    if (!UserSettings::instance()->autoUpdateTranslations()) {
        return;
    }

    if (m_activeTranslationDownloads.isEmpty()) {
        downloadLatestTranslations();
    }
}

void Updater::downloadLatestTranslations()
{
    if (m_translationDownloading)
        return;
    ++m_translationGeneration;
    m_translationDownloading = true;
    emit translationDownloadingChanged();

    m_totalTranslationDownloads = 0;
    m_completedTranslationDownloads = 0;
    m_failedTranslationDownloads = 0;

    QStringList languageCodes = getLanguageCodes();
    QString baseUrl = "https://raw.githubusercontent.com/ChrisLauinger77/QontrolPanel/main/i18n_compiled/QontrolPanel_%1.qm";

    m_totalTranslationDownloads = languageCodes.size() + 1;
    emit translationDownloadStarted();

    for (const QString& langCode : languageCodes) {
        QString url = baseUrl.arg(langCode);
        downloadTranslationFile(langCode, url);
    }

    downloadTranslationProgressFile();

    if (m_totalTranslationDownloads == 0) {
        emit translationDownloadFinished(false, tr("No translation files to download"));
    }
}

void Updater::cancelTranslationDownload()
{
    ++m_translationGeneration;
    cancelReplyBatch(m_activeTranslationDownloads, this);
    if (m_translationDownloading)
    {
        m_translationDownloading = false;
        emit translationDownloadingChanged();
        emit translationDownloadFinished(false, tr("Download cancelled"));
        }
}

void Updater::downloadTranslationFile(const QString& languageCode, const QString& githubUrl)
{
    QUrl url(githubUrl);
    QNetworkRequest request;
    request.setUrl(url);

    QString userAgent = QString("QontrolPanel/%1").arg(APP_VERSION_STRING);
    request.setRawHeader("User-Agent", userAgent.toUtf8());
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    request.setRawHeader("Accept", "*/*");
    request.setRawHeader("Connection", "keep-alive");
    request.setTransferTimeout(30000);

    QNetworkReply* reply = m_networkManager->get(request);
    reply->setProperty("languageCode", languageCode);
    reply->setProperty("translationGeneration", QVariant::fromValue(m_translationGeneration));
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64 total) {
        if (received > 4 * 1024 * 1024 || total > 4 * 1024 * 1024)
            reply->abort();
    });
    m_activeTranslationDownloads.append(reply);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, languageCode](qint64 bytesReceived, qint64 bytesTotal) {
                emit translationDownloadProgress(languageCode,
                                                 static_cast<int>(bytesReceived),
                                                 static_cast<int>(bytesTotal));
            });

    connect(reply, &QNetworkReply::finished, this, &Updater::onTranslationFileDownloaded);

    connect(reply, &QNetworkReply::errorOccurred, this,
            [this, languageCode](QNetworkReply::NetworkError error) {
                qWarning() << "Network error for" << languageCode << ":" << error;
            });
}

void Updater::onTranslationFileDownloaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->property("translationGeneration").toULongLong() != m_translationGeneration)
    {
        reply->deleteLater();
        return;
    }
    const QString languageCode = reply->property("languageCode").toString();
    m_activeTranslationDownloads.removeAll(reply);
    bool success = false;
    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray data = reply->readAll();
        const bool metadata = languageCode == "translation_progress";
        QTranslator validator;
        const QJsonDocument document = metadata ? QJsonDocument::fromJson(data) : QJsonDocument{};
        const bool valid = !data.isEmpty() && data.size() <= 4 * 1024 * 1024 &&
                           (metadata ? document.isObject()
                                     : validator.load(reinterpret_cast<const uchar*>(data.constData()), data.size()));
        if (valid)
        {
            QDir().mkpath(getTranslationDownloadPath());
            QSaveFile file(metadata ? getTranslationProgressPath()
                                    : getTranslationDownloadPath() + "/QontrolPanel_" + languageCode + ".qm");
            success = file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
            if (success && metadata)
                loadTranslationProgressData();
            }
        }
    if (!success)
    {
        ++m_failedTranslationDownloads;
        LOG_WARN("Updater", QString("Translation update failed validation or saving: %1").arg(languageCode));
    }

    m_completedTranslationDownloads++;
    emit translationFileCompleted(languageCode, m_completedTranslationDownloads, m_totalTranslationDownloads);

    if (m_completedTranslationDownloads >= m_totalTranslationDownloads) {
        bool success = (m_failedTranslationDownloads == 0);
        QString message;

        if (success) {
            message = tr("All translations downloaded successfully");
        } else {
            message = tr("Downloaded %1 of %2 translation files")
            .arg(m_totalTranslationDownloads - m_failedTranslationDownloads)
                .arg(m_totalTranslationDownloads);
        }

        m_translationDownloading = false;
        emit translationDownloadingChanged();
        emit translationDownloadFinished(success, message);
    }

    reply->deleteLater();
}

QString Updater::getTranslationDownloadPath() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    return appDir + "/i18n";
}

QString Updater::getTranslationProgressPath() const
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath);
    return appDataPath + "/translation_progress.json";
}

void Updater::loadTranslationProgressData()
{
    QString progressFilePath = getTranslationProgressPath();
    LOG_INFO("Updater",
             QString("Loading translation progress data from: %1").arg(progressFilePath));

    QFile file(progressFilePath);
    if (!file.exists()) {
        LOG_WARN("Updater",
                 QString("Translation progress file does not exist: %1").arg(progressFilePath));
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        LOG_CRITICAL("Updater",
                     QString("Failed to open translation progress file: %1").arg(file.errorString()));
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        LOG_CRITICAL("Updater",
                     "Failed to parse translation progress JSON - invalid format");
        return;
    }

    m_translationProgress = doc.object();
    LOG_INFO("Updater",
             "Translation progress data loaded successfully");
    emit translationProgressDataLoaded();
}

void Updater::downloadTranslationProgressFile()
{
    downloadTranslationFile(
        "translation_progress",
        "https://raw.githubusercontent.com/ChrisLauinger77/QontrolPanel/main/i18n_compiled/translation_progress.json");
}

int Updater::getTranslationProgress(const QString& languageCode)
{
    if (m_translationProgress.contains(languageCode)) {
        QJsonValue value = m_translationProgress[languageCode];
        // Handle both old format (int) and new format (object with percentage)
        if (value.isDouble()) {
            return value.toInt();
        } else if (value.isObject()) {
            return value.toObject()["percentage"].toInt();
        }
    }
    return 0;
}

QString Updater::getTranslationLastUpdated(const QString& languageCode)
{
    if (m_translationProgress.contains(languageCode)) {
        QJsonValue value = m_translationProgress[languageCode];
        if (value.isObject()) {
            QString dateStr = value.toObject()["last_updated"].toString();
            if (!dateStr.isEmpty()) {
                return formatLocalizedTimestamp(dateStr);
            }
        }
    }
    return QString();
}

QString Updater::getTranslationContributor(const QString& languageCode)
{
    if (m_translationProgress.contains(languageCode)) {
        QJsonValue value = m_translationProgress[languageCode];
        if (value.isObject()) {
            QString contributor = value.toObject()["contributor"].toString();
            if (!contributor.isEmpty()) {
                return contributor;
            }
        }
    }
    return QString();
}

bool Updater::hasTranslationProgressData()
{
    return !m_translationProgress.isEmpty();
}

void Updater::checkForAppUpdatesTimer()
{
    if (!UserSettings::instance()->autoFetchForAppUpdates()) {
        return;
    }

    checkForAppUpdatesAuto();
}

void Updater::checkForAppUpdatesAuto()
{
    if (!UserSettings::instance()->autoFetchForAppUpdates()) {
        return;
    }

    if (m_isChecking || m_isDownloading) {
        return;
    }

    connect(this, &Updater::updateFinished, this,
            [this](bool success, const QString& message) {
                disconnect(this, &Updater::updateFinished, this, nullptr);

                if (success && m_updateAvailable) {
                    emit updateAvailableNotification(m_latestVersion);
                }
            }, Qt::SingleShotConnection);

    checkForUpdates();
}

QString Updater::getAppVersion() const
{
    return APP_VERSION_STRING;
}

QString Updater::getQtVersion() const
{
    return QT_VERSION_STRING;
}

QString Updater::getCommitHash() const
{
    return QString(GIT_COMMIT_HASH);
}

QString Updater::getHeadsetControlVersion() const
{
    return QString::fromStdString(std::string(headsetcontrol::version()));
}

QString Updater::getBuildTimestamp() const
{
    return formatLocalizedTimestamp(QString(BUILD_TIMESTAMP));
}

void Updater::writeDownloadChunk(QNetworkReply* reply)
{
    if (!m_installerFile || !m_downloadError.isEmpty())
        return;
    while (reply->bytesAvailable() > 0)
    {
        const QByteArray chunk = reply->read(64 * 1024);
        m_receivedSize += chunk.size();
        if (m_receivedSize > m_expectedSize || m_installerFile->write(chunk) != chunk.size())
        {
            m_downloadError = tr("The download exceeded its expected size or could not be saved.");
            if (reply->isRunning())
                reply->abort();
            return;
        }
        m_downloadHash.addData(chunk);
    }
}

Updater::~Updater()
{
    cancelTranslationDownload();
    for (auto* reply : m_networkManager->findChildren<QNetworkReply*>())
    {
        disconnect(reply, nullptr, this, nullptr);
        reply->abort();
    }
    m_instance = nullptr;
}
