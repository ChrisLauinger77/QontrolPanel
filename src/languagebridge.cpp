#include "languagebridge.h"
#include "languages.h"
#include "updater.h"
#include "usersettings.h"
#include <QCoreApplication>
#include <QDebug>
#include <QLocale>

LanguageBridge* LanguageBridge::m_instance = nullptr;

LanguageBridge* LanguageBridge::instance()
{
    if (!m_instance) {
        m_instance = new LanguageBridge();
    }
    return m_instance;
}

LanguageBridge* LanguageBridge::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    return instance();
}

LanguageBridge::LanguageBridge(QObject *parent)
    : QObject(parent)
    , translator(new QTranslator(this))
{
    connect(UserSettings::instance(), &UserSettings::languageIndexChanged,
            this, &LanguageBridge::reloadApplicationLanguage);
    connect(Updater::instance(), &Updater::translationDownloadFinished,
            this, &LanguageBridge::reloadApplicationLanguage);
}

QString LanguageBridge::getCurrentLanguageCode() const {
    QLocale locale;
    QString languageCode = locale.name().section('_', 0, 0);

    if (languageCode == "zh") {
        QString fullLocale = locale.name();
        if (fullLocale.startsWith("zh_CN")) {
            return "zh_CN";
        }
        return "zh_CN";
    }

    return languageCode;
}

void LanguageBridge::reloadApplicationLanguage()
{
    const int languageIndex = UserSettings::instance()->languageIndex();
    QCoreApplication::removeTranslator(translator);
    delete translator;
    translator = new QTranslator(this);

    QString languageCode;
    if (languageIndex == 0) {
        languageCode = getCurrentLanguageCode();
    } else {
        languageCode = getLanguageCodeFromIndex(languageIndex);
    }

    QString translationFile = QString(QCoreApplication::applicationDirPath() + "/i18n/QontrolPanel_%1.qm").arg(languageCode);
    if (translator->load(translationFile)) {
        QCoreApplication::installTranslator(translator);
    } else {
        qWarning() << "Failed to load translation file:" << translationFile;
    }

    emit languageChanged();
}

QString LanguageBridge::getLanguageCodeFromIndex(int index) const
{
    if (index == 0) {
        QLocale systemLocale;
        QString langCode = systemLocale.name().section('_', 0, 0);

        if (langCode == "zh") {
            QString fullLocale = systemLocale.name();
            if (fullLocale.startsWith("zh_CN")) {
                return "zh_CN";
            }
            return "zh_CN";
        }

        return langCode;
    }

    auto languages = getSupportedLanguages();
    if (index > 0 && index <= languages.size()) {
        return languages[index - 1].code;
    }

    return "en";
}

QStringList LanguageBridge::getLanguageNativeNames() const
{
    auto names = ::getLanguageNativeNames();
    return names;
}

LanguageBridge::~LanguageBridge()
{
    QCoreApplication::removeTranslator(translator);
    m_instance = nullptr;
}
