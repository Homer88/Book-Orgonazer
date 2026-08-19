#include "settingsdialog.h"
#include "settings.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Настройки"));
    resize(500, 420);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(buildForm());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                         QDialogButtonBox::Cancel);
    buttons->addButton(QStringLiteral("Применить"), QDialogButtonBox::ApplyRole);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("ОК"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));

    auto saveAndApply = [this]() {
        auto &s = SettingsManager::instance();
        s.setFontFamily(m_fontCombo->currentFont().family());
        s.setFontSize(m_sizeSpin->value());
        s.setScale(m_scaleCombo->currentData().toDouble());
        s.save();
        s.load();
        QApplication::setFont(s.appFont());
    };

    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked,
            this, [this, saveAndApply]() { saveAndApply(); accept(); });
    connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
            this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, saveAndApply);
    layout->addWidget(buttons);
}

QWidget *SettingsDialog::buildForm()
{
    auto *host = new QWidget;
    auto *outer = new QVBoxLayout(host);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *fontGroup = new QGroupBox(QStringLiteral("Шрифт"));
    auto *fontForm = new QFormLayout(fontGroup);

    auto &s = SettingsManager::instance();

    m_fontCombo = new QFontComboBox;
    m_fontCombo->setCurrentFont(QFont(s.fontFamily()));
    fontForm->addRow(QStringLiteral("Семейство:"), m_fontCombo);

    m_sizeSpin = new QSpinBox;
    m_sizeSpin->setRange(6, 36);
    m_sizeSpin->setValue(s.fontSize());
    fontForm->addRow(QStringLiteral("Размер:"), m_sizeSpin);

    auto *scaleGroup = new QGroupBox(QStringLiteral("Масштаб / Разрешение"));
    auto *scaleForm = new QFormLayout(scaleGroup);

    m_scaleCombo = new QComboBox;
    m_scaleCombo->addItem(QStringLiteral("75%   (1280×720)"),    0.75);
    m_scaleCombo->addItem(QStringLiteral("85%   (1280×1024)"),   0.85);
    m_scaleCombo->addItem(QStringLiteral("90%   (1366×768)"),    0.9);
    m_scaleCombo->addItem(QStringLiteral("95%   (1600×900)"),    0.95);
    m_scaleCombo->addItem(QStringLiteral("100%  (1920×1080)"),   1.0);
    m_scaleCombo->addItem(QStringLiteral("125%  (2560×1440)"),   1.25);
    m_scaleCombo->addItem(QStringLiteral("150%  (3200×1800)"),   1.5);
    m_scaleCombo->addItem(QStringLiteral("200%  (3840×2160)"),   2.0);
    double curScale = s.scale();
    for (int i = 0; i < m_scaleCombo->count(); ++i) {
        if (qFuzzyCompare(m_scaleCombo->itemData(i).toDouble(), curScale)) {
            m_scaleCombo->setCurrentIndex(i);
            break;
        }
    }
    scaleForm->addRow(QStringLiteral("Масштаб:"), m_scaleCombo);

    auto *previewGroup = new QGroupBox(QStringLiteral("Предпросмотр"));
    auto *previewLayout = new QVBoxLayout(previewGroup);
    m_preview = new QLabel(QStringLiteral("Пример текста — Книга 123\n"
                                         "Абвгдеёжзийклмнопрстуфхцчшщъыьэюя"));
    m_preview->setFrameShape(QLabel::Box);
    m_preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_preview->setMinimumHeight(80);
    previewLayout->addWidget(m_preview);

    outer->addWidget(fontGroup);
    outer->addWidget(scaleGroup);
    outer->addWidget(previewGroup);
    outer->addStretch(1);

    connect(m_fontCombo, &QFontComboBox::currentFontChanged,
            this, &SettingsDialog::updatePreview);
    connect(m_sizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsDialog::updatePreview);
    connect(m_scaleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::updatePreview);

    updatePreview();
    return host;
}

void SettingsDialog::updatePreview()
{
    double scale = m_scaleCombo->currentData().toDouble();
    QFont f(m_fontCombo->currentFont().family(),
            qRound(m_sizeSpin->value() * scale));
    m_preview->setFont(f);
}
