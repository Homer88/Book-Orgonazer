#pragma once

#include <QString>
#include <QFont>

class SettingsManager
{
public:
    static SettingsManager &instance();

    void load();
    void save();

    QFont appFont() const;
    double scale() const;
    int maxFormHeight() const;

    void setFontFamily(const QString &family);
    void setFontSize(int size);
    void setScale(double s);

    QString fontFamily() const;
    int fontSize() const;

private:
    SettingsManager() = default;
    QString settingsPath() const;

    QString m_fontFamily;
    int m_fontSize = 10;
    double m_scale = 1.0;
};
