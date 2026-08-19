#include "settings.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>
#include <QFile>
#include <QSettings>

SettingsManager &SettingsManager::instance()
{
    static SettingsManager s;
    return s;
}

QString SettingsManager::settingsPath() const
{
    return QApplication::applicationDirPath() +
           QStringLiteral("/ui_settings.ini");
}

void SettingsManager::load()
{
    QSettings ini(settingsPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("UI"));
    m_fontFamily = ini.value(QStringLiteral("fontFamily"),
                             QStringLiteral("Segoe UI")).toString();
    m_fontSize = ini.value(QStringLiteral("fontSize"), 10).toInt();
    m_scale = ini.value(QStringLiteral("scale"), 1.0).toDouble();
    if (m_scale < 0.5)
        m_scale = 0.5;
    if (m_scale > 3.0)
        m_scale = 3.0;
    ini.endGroup();
}

void SettingsManager::save()
{
    QSettings ini(settingsPath(), QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("UI"));
    ini.setValue(QStringLiteral("fontFamily"), m_fontFamily);
    ini.setValue(QStringLiteral("fontSize"), m_fontSize);
    ini.setValue(QStringLiteral("scale"), m_scale);
    ini.endGroup();
    ini.sync();
}

QFont SettingsManager::appFont() const
{
    QFont f(m_fontFamily, qRound(m_fontSize * m_scale));
    return f;
}

double SettingsManager::scale() const { return m_scale; }

int SettingsManager::maxFormHeight() const
{
    int screenH = QApplication::desktop()->availableGeometry().height();
    return qMax(400, screenH - 80);
}

void SettingsManager::setFontFamily(const QString &family) { m_fontFamily = family; }
void SettingsManager::setFontSize(int size) { m_fontSize = size; }
void SettingsManager::setScale(double s) { m_scale = s; }

QString SettingsManager::fontFamily() const { return m_fontFamily; }
int SettingsManager::fontSize() const { return m_fontSize; }
