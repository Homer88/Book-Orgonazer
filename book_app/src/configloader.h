#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

class ConfigLoader
{
public:
    ConfigLoader() = default;
    explicit ConfigLoader(const QString &filePath) { load(filePath); }

    bool load(const QString &filePath);

    bool isValid() const { return m_valid; }
    QString errorString() const { return m_error; }

    QStringList values(const QString &section) const;
    QString value(const QString &section, const QString &key) const;

    QString basePath() const { return m_basePath; }

private:
    bool m_valid = false;
    QString m_error;
    QString m_basePath;
    QMap<QString, QStringList> m_sections;
};
