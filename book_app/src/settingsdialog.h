#pragma once

#include <QDialog>

class QFontComboBox;
class QComboBox;
class QSpinBox;
class QLabel;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void updatePreview();

private:
    QWidget *buildForm();

    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_sizeSpin = nullptr;
    QComboBox *m_scaleCombo = nullptr;
    QLabel *m_preview = nullptr;
};
