#include "configloader.h"

#include <QFile>
#include <QTextCodec>
#include <QTextStream>

bool ConfigLoader::load(const QString &filePath)
{
    m_valid = false;
    m_error.clear();
    m_basePath.clear();
    m_sections.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_error = QStringLiteral("Не удалось открыть файл конфигурации: ") + filePath;
        return false;
    }

    const QByteArray data = file.readAll();
    file.close();

    QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
    if (!codec)
        codec = QTextCodec::codecForLocale();
    const QString text = codec->toUnicode(data);

    QString currentSection;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')))
            continue;

        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            currentSection = line.mid(1, line.length() - 2).trimmed();
            continue;
        }

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0 || currentSection.isEmpty())
            continue;

        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (currentSection == QLatin1String("part") && key == QLatin1String("param1")) {
            m_basePath = value;
            continue;
        }

        QStringList &list = m_sections[currentSection];
        if ((key.startsWith(QLatin1String("param")) ||
             key.startsWith(QLatin1String("file"))) &&
            !value.isEmpty())
            list.append(value);
    }

    m_valid = !m_sections.isEmpty() || !m_basePath.isEmpty();
    return m_valid;
}

QStringList ConfigLoader::values(const QString &section) const
{
    return m_sections.value(section);
}

QString ConfigLoader::value(const QString &section, const QString &key) const
{
    return m_sections.value(section).value(0);
}
