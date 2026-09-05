#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QTranslator>

class LanguageBridge : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    ~LanguageBridge() override;
    static LanguageBridge* instance();
    static LanguageBridge* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    Q_INVOKABLE void changeApplicationLanguage(int languageIndex);
    Q_INVOKABLE QString getLanguageCodeFromIndex(int index) const;
    Q_INVOKABLE QStringList getLanguageNativeNames() const;

signals:
    void languageChanged();

private:
    explicit LanguageBridge(QObject *parent = nullptr);
    static LanguageBridge* m_instance;

    QTranslator *translator;
    QString getCurrentLanguageCode() const;
};
