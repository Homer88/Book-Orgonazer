#include "editbookdialog.h"
#include "langtable.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextCodec>
#include <QVBoxLayout>

namespace {
void fillCombo(QComboBox *combo, const QStringList &items)
{
    combo->addItems(items);
    combo->setEditable(true);
}

QString bytesToSizeText(qint64 bytes)
{
    return QStringLiteral("%1 кб").arg(qRound64(bytes / 1024.0));
}

QString buildDialogFilter(const QStringList &fileFilters)
{
    QStringList result;
    for (const QString &entry : fileFilters) {
        const QStringList parts = entry.split(QLatin1Char('|'));
        for (int i = 0; i + 1 < parts.size(); i += 2)
            result << parts.at(i) + QStringLiteral(" (") + parts.at(i + 1) +
                          QLatin1Char(')');
    }
    return result.isEmpty() ? QStringLiteral("Все файлы (*.*)")
                            : result.join(QStringLiteral(";;"));
}

QString bookDirFromPart(const QString &baseDir, const QString &part)
{
    QString rel = part.trimmed();
    if (rel.isEmpty())
        return QString();
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (rel.startsWith(QLatin1Char('/')))
        rel.remove(0, 1);
    return QDir(baseDir).absoluteFilePath(rel);
}
} // namespace

EditBookDialog::EditBookDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Редактировать книгу"));
    resize(600, 640);

    auto *layout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(buildForm());
    layout->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                         QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Сохранить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void EditBookDialog::setBookIndex(int index)
{
    m_index = index;
}

void EditBookDialog::setBaseDir(const QString &dir)
{
    m_baseDir = dir;
}

void EditBookDialog::setFileFilters(const QStringList &filters)
{
    m_fileFilters = filters;
}

void EditBookDialog::setReferenceData(const QStringList &formats,
                                      const QStringList &languages,
                                      const QStringList &temas,
                                      const QStringList &systems,
                                      const QStringList &studios,
                                      const QStringList &types,
                                      const QStringList &sources)
{
    if (m_formatCombo) {
        fillCombo(m_formatCombo, formats);
        fillCombo(m_langCombo, languages);
        fillCombo(m_temaCombo, temas);
        fillCombo(m_systemCombo, systems);
        fillCombo(m_studioCombo, studios);
        fillCombo(m_vidCombo, types);
        fillCombo(m_sourceCombo, sources);
    }
}

QWidget *EditBookDialog::buildForm()
{
    auto *scrollHost = new QWidget(this);
    auto *outer = new QVBoxLayout(scrollHost);

    auto *fileBox = new QGroupBox(QStringLiteral("Файл книги"));
    auto *fileForm = new QFormLayout(fileBox);

    auto *browseRow = new QHBoxLayout;
    m_fileEdit = new QLineEdit;
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(QStringLiteral("Заменить файл книги..."));
    auto *browseButton = new QPushButton(QStringLiteral("Обновить..."));
    connect(browseButton, &QPushButton::clicked, this, &EditBookDialog::browseNewFile);
    browseRow->addWidget(m_fileEdit, 1);
    browseRow->addWidget(browseButton);
    fileForm->addRow(QStringLiteral("Новый файл:"), browseRow);

    m_sizeEdit = new QLineEdit;
    m_sizeEdit->setReadOnly(true);
    m_crcEdit = new QLineEdit;
    m_crcEdit->setReadOnly(true);
    fileForm->addRow(QStringLiteral("Размер:"), m_sizeEdit);
    fileForm->addRow(QStringLiteral("CRC:"), m_crcEdit);

    auto *mainBox = new QGroupBox(QStringLiteral("Основные сведения"));
    auto *mainForm = new QFormLayout(mainBox);

    m_nameEdit = new QLineEdit;
    m_authorEdit = new QLineEdit;
    m_yearSpin = new QSpinBox;
    m_yearSpin->setRange(0, 9999);
    m_yearSpin->setSpecialValueText(QStringLiteral("—"));

    m_formatCombo = new QComboBox;
    m_langCombo = new QComboBox;
    m_temaCombo = new QComboBox;
    m_systemCombo = new QComboBox;
    m_studioCombo = new QComboBox;
    m_vidCombo = new QComboBox;
    m_sourceCombo = new QComboBox;

    m_pagesSpin = new QSpinBox;
    m_pagesSpin->setRange(0, 100000);
    m_pagesSpin->setSpecialValueText(QStringLiteral("—"));
    m_izdatelEdit = new QLineEdit;

    auto *langBox = new QGroupBox(QStringLiteral("Языки программирования"));
    auto *langLayout = new QGridLayout(langBox);
    for (int i = 0; i < kLangCount; ++i) {
        auto *check = new QCheckBox(QString::fromUtf8(kLangTable[i].display));
        m_langChecks.append(check);
        langLayout->addWidget(check, i / 3, i % 3);
    }

    mainForm->addRow(QStringLiteral("Название:"), m_nameEdit);
    mainForm->addRow(QStringLiteral("Автор:"), m_authorEdit);
    mainForm->addRow(QStringLiteral("Год:"), m_yearSpin);
    mainForm->addRow(QStringLiteral("Формат:"), m_formatCombo);
    mainForm->addRow(QStringLiteral("Язык книги:"), m_langCombo);
    mainForm->addRow(QStringLiteral("Тематика:"), m_temaCombo);
    mainForm->addRow(QStringLiteral("Система:"), m_systemCombo);
    mainForm->addRow(QStringLiteral("Студия:"), m_studioCombo);
    mainForm->addRow(QStringLiteral("Тип:"), m_vidCombo);
    mainForm->addRow(QStringLiteral("Наличие:"), m_sourceCombo);
    mainForm->addRow(QStringLiteral("Страниц:"), m_pagesSpin);
    mainForm->addRow(QStringLiteral("Издатель:"), m_izdatelEdit);

    auto *pathBox = new QGroupBox(QStringLiteral("Файлы и путь"));
    auto *pathForm = new QFormLayout(pathBox);

    m_partEdit = new QLineEdit;
    m_partEdit->setPlaceholderText(QStringLiteral("Например: book\\ID\\"));
    m_partEdit->setReadOnly(true);
    m_imageEdit = new QLineEdit;
    m_imageEdit->setPlaceholderText(QStringLiteral("cover.jpg"));
    m_imageEdit->setReadOnly(true);

    pathForm->addRow(QStringLiteral("Путь (part):"), m_partEdit);
    pathForm->addRow(QStringLiteral("Обложка:"), m_imageEdit);

    auto *descBox = new QGroupBox(QStringLiteral("Описание"));
    auto *descLayout = new QVBoxLayout(descBox);
    m_descEdit = new QPlainTextEdit;
    m_descEdit->setPlaceholderText(QStringLiteral("Описание книги (descripter.txt)"));
    m_descEdit->setMaximumHeight(150);
    descLayout->addWidget(m_descEdit);

    outer->addWidget(fileBox);
    outer->addWidget(mainBox);
    outer->addWidget(langBox);
    outer->addWidget(pathBox);
    outer->addWidget(descBox);
    outer->addStretch(1);
    return scrollHost;
}

void EditBookDialog::loadBook()
{
    QSqlQuery query(QSqlDatabase::database(QStringLiteral("book")));
    query.prepare(QStringLiteral("SELECT name_book, ahtor, yaer, format, lang, "
                                 "lang_string, langav, tematica, system, studio, "
                                 "vid, source, pages, izdatel, part, image, "
                                 "size, crc "
                                 "FROM book WHERE [index] = ?"));
    query.addBindValue(m_index);
    if (!query.exec() || !query.next())
        return;

    m_nameEdit->setText(query.value(0).toString());
    m_authorEdit->setText(query.value(1).toString());
    m_oldAuthor = query.value(1).toString();
    m_yearSpin->setValue(query.value(2).toInt());
    m_formatCombo->setCurrentText(query.value(3).toString());
    m_oldFormat = query.value(3).toString();
    setLangChecks(query.value(4).toInt());
    m_langCombo->setCurrentText(query.value(6).toString());
    m_temaCombo->setCurrentText(query.value(7).toString());
    m_systemCombo->setCurrentText(query.value(8).toString());
    m_studioCombo->setCurrentText(query.value(9).toString());
    m_vidCombo->setCurrentText(query.value(10).toString());
    m_sourceCombo->setCurrentText(query.value(11).toString());
    m_pagesSpin->setValue(query.value(12).toInt());
    m_izdatelEdit->setText(query.value(13).toString());
    m_partEdit->setText(query.value(14).toString());
    m_imageEdit->setText(query.value(15).toString());
    m_sizeEdit->setText(query.value(16).toString());
    m_crcEdit->setText(query.value(17).toString());

    const QString dir = bookDirFromPart(m_baseDir, query.value(14).toString());
    const QString descPath = QDir(dir).filePath(QStringLiteral("descripter.txt"));
    QFile desc(descPath);
    if (desc.open(QIODevice::ReadOnly)) {
        const QByteArray raw = desc.readAll();
        QString text = QString::fromUtf8(raw);
        if (text.contains(QChar::ReplacementCharacter)) {
            if (QTextCodec *codec = QTextCodec::codecForName("Windows-1251"))
                text = codec->toUnicode(raw);
        }
        m_descEdit->setPlainText(text);
    }
}

void EditBookDialog::setLangChecks(int code)
{
    for (int i = 0; i < m_langChecks.size() && i < kLangCount; ++i)
        m_langChecks.at(i)->setChecked((code / kLangTable[i].code) % 10 == 1);
}

int EditBookDialog::langCode() const
{
    int code = 0;
    for (int i = 0; i < m_langChecks.size() && i < kLangCount; ++i) {
        if (m_langChecks.at(i)->isChecked())
            code += kLangTable[i].code;
    }
    return code;
}

QString EditBookDialog::langString() const
{
    QStringList names;
    for (int i = 0; i < m_langChecks.size() && i < kLangCount; ++i) {
        if (m_langChecks.at(i)->isChecked())
            names << QString::fromUtf8(kLangTable[i].stored);
    }
    return names.join(QStringLiteral(", "));
}

void EditBookDialog::browseNewFile()
{
    const QString filter = buildDialogFilter(m_fileFilters);
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Выберите новый файл книги"), QString(), filter);
    if (file.isEmpty())
        return;

    const QString md5 = md5OfFile(file);
    if (md5.isEmpty())
        return;

    if (!m_crcEdit->text().trimmed().isEmpty() &&
        m_crcEdit->text().trimmed() == md5) {
        QMessageBox::information(this, QStringLiteral("Файл не изменился"),
                                 QStringLiteral("Это тот же файл, что уже прикреплён "
                                                "к книге."));
        return;
    }

    QSqlQuery check(QSqlDatabase::database(QStringLiteral("book")));
    check.prepare(QStringLiteral("SELECT [index] FROM book WHERE crc = ?"));
    check.addBindValue(md5);
    if (check.exec() && check.next()) {
        const int otherId = check.value(0).toInt();
        if (otherId != m_index) {
            QMessageBox::warning(this, QStringLiteral("Дубликат"),
                                 QStringLiteral("Файл с такой контрольной суммой "
                                                "уже привязан к книге ID %1.")
                                     .arg(otherId));
            return;
        }
    }

    m_selectedFile = file;
    m_md5 = md5;
    m_fileEdit->setText(file);
    m_sizeEdit->setText(bytesToSizeText(QFileInfo(file).size()));
    m_crcEdit->setText(md5);

    const QString suffix = QFileInfo(file).suffix();
    if (!suffix.isEmpty())
        m_formatCombo->setCurrentText(QLatin1Char('.') + suffix);
}

QString EditBookDialog::md5OfFile(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Md5);
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (!file.atEnd()) {
        const qint64 n = file.read(buffer.data(), buffer.size());
        if (n <= 0)
            break;
        hash.addData(buffer.constData(), n);
    }
    return QString::fromLatin1(hash.result().toHex()).toUpper();
}

QString EditBookDialog::commit()
{
    QString size = m_sizeEdit->text().trimmed();
    QString crc = m_crcEdit->text().trimmed();
    QString format = m_formatCombo->currentText().trimmed();

    const QString part = m_partEdit->text().trimmed();

    if (!m_selectedFile.isEmpty()) {
        const QString dir = bookDirFromPart(m_baseDir, part);
        if (dir.isEmpty())
            return QStringLiteral("Не указан путь к книге (part).");
        const QDir targetDir(dir);
        if (!targetDir.exists())
            return QStringLiteral("Папка книги не существует:\n%1")
                .arg(targetDir.absolutePath());

        const QString newFileName = m_authorEdit->text().trimmed() + format;
        const QString destFile = targetDir.filePath(newFileName);
        if (QFile::exists(destFile)) {
            if (!QFile::remove(destFile))
                return QStringLiteral("Не удалось заменить существующий файл:\n%1")
                    .arg(destFile);
        }
        if (!QFile::copy(m_selectedFile, destFile))
            return QStringLiteral("Не удалось скопировать новый файл:\n%1")
                .arg(destFile);

        const QString oldFileName = m_oldAuthor.trimmed() + m_oldFormat.trimmed();
        if (!oldFileName.isEmpty() && oldFileName != newFileName) {
            const QString oldFile = targetDir.filePath(oldFileName);
            if (QFile::exists(oldFile))
                QFile::remove(oldFile);
        }

        QFile::remove(m_selectedFile);
        size = bytesToSizeText(QFileInfo(destFile).size());
        crc = m_md5;
    }

    QSqlQuery query(QSqlDatabase::database(QStringLiteral("book")));
    query.prepare(QStringLiteral(
        "UPDATE book SET name_book = ?, ahtor = ?, yaer = ?, format = ?, "
        "lang = ?, lang_string = ?, langav = ?, tematica = ?, system = ?, "
        "studio = ?, vid = ?, source = ?, pages = ?, izdatel = ?, part = ?, "
        "image = ?, size = ?, crc = ? WHERE [index] = ?"));
    query.addBindValue(m_nameEdit->text().trimmed());
    query.addBindValue(m_authorEdit->text().trimmed());
    query.addBindValue(m_yearSpin->value() == 0 ? QVariant() : QVariant(m_yearSpin->value()));
    query.addBindValue(format);
    query.addBindValue(langCode());
    query.addBindValue(langString());
    query.addBindValue(m_langCombo->currentText().trimmed());
    query.addBindValue(m_temaCombo->currentText().trimmed());
    query.addBindValue(m_systemCombo->currentText().trimmed());
    query.addBindValue(m_studioCombo->currentText().trimmed());
    query.addBindValue(m_vidCombo->currentText().trimmed());
    query.addBindValue(m_sourceCombo->currentText().trimmed());
    query.addBindValue(m_pagesSpin->value() == 0 ? QVariant() : QVariant(m_pagesSpin->value()));
    query.addBindValue(m_izdatelEdit->text().trimmed());
    query.addBindValue(part);
    query.addBindValue(m_imageEdit->text().trimmed());
    query.addBindValue(size);
    query.addBindValue(crc);
    query.addBindValue(m_index);
    if (!query.exec())
        return QStringLiteral("Не удалось сохранить книгу:\n%1")
            .arg(query.lastError().text());

    const QString dir = bookDirFromPart(m_baseDir, part);
    const QString descPath = QDir(dir).filePath(QStringLiteral("descripter.txt"));
    QFile desc(descPath);
    if (desc.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        desc.write(m_descEdit->toPlainText().toUtf8());
        desc.close();
    }

    return QString();
}
