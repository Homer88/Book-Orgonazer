#include "addbookdialog.h"
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
} // namespace

AddBookDialog::AddBookDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Добавить книгу"));
    resize(600, 680);

    auto *layout = new QVBoxLayout(this);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidget(buildForm());
    layout->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                         QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Добавить"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void AddBookDialog::setBaseDir(const QString &dir)
{
    m_baseDir = dir;
    if (m_partEdit && m_partEdit->text().isEmpty())
        m_partEdit->setText(QStringLiteral("book\\"));
}

void AddBookDialog::setNoImagePath(const QString &path)
{
    m_noImagePath = path;
}

void AddBookDialog::setFileFilters(const QStringList &filters)
{
    m_fileFilters = filters;
}

void AddBookDialog::setReferenceData(const QStringList &formats,
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

QWidget *AddBookDialog::buildForm()
{
    auto *scrollHost = new QWidget(this);
    auto *outer = new QVBoxLayout(scrollHost);

    auto *fileBox = new QGroupBox(QStringLiteral("Файл книги"));
    auto *fileForm = new QFormLayout(fileBox);

    auto *browseRow = new QHBoxLayout;
    m_fileEdit = new QLineEdit;
    m_fileEdit->setReadOnly(true);
    m_fileEdit->setPlaceholderText(QStringLiteral("Выберите файл книги..."));
    auto *browseButton = new QPushButton(QStringLiteral("Обзор..."));
    connect(browseButton, &QPushButton::clicked, this, &AddBookDialog::browseBook);
    browseRow->addWidget(m_fileEdit, 1);
    browseRow->addWidget(browseButton);
    fileForm->addRow(QStringLiteral("Файл:"), browseRow);

    auto *coverRow = new QHBoxLayout;
    m_coverFileEdit = new QLineEdit;
    m_coverFileEdit->setReadOnly(true);
    m_coverFileEdit->setPlaceholderText(QStringLiteral("Не обязательно"));
    auto *coverButton = new QPushButton(QStringLiteral("Обзор..."));
    connect(coverButton, &QPushButton::clicked, this, &AddBookDialog::browseCover);
    coverRow->addWidget(m_coverFileEdit, 1);
    coverRow->addWidget(coverButton);
    fileForm->addRow(QStringLiteral("Обложка:"), coverRow);

    auto *mainBox = new QGroupBox(QStringLiteral("Основные сведения"));
    auto *mainForm = new QFormLayout(mainBox);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(QStringLiteral("Название книги"));
    m_authorEdit = new QLineEdit;
    m_authorEdit->setPlaceholderText(QStringLiteral("Автор"));
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

    auto *pathBox = new QGroupBox(QStringLiteral("Данные файла"));
    auto *pathForm = new QFormLayout(pathBox);

    m_sizeEdit = new QLineEdit;
    m_sizeEdit->setPlaceholderText(QStringLiteral("Размер файла"));
    m_sizeEdit->setReadOnly(true);
    m_crcEdit = new QLineEdit;
    m_crcEdit->setPlaceholderText(QStringLiteral("MD5"));
    m_crcEdit->setReadOnly(true);
    m_partEdit = new QLineEdit;
    m_partEdit->setPlaceholderText(QStringLiteral("Например: book\\ID\\"));
    m_partEdit->setReadOnly(true);
    m_imageEdit = new QLineEdit;
    m_imageEdit->setText(QStringLiteral("cover.jpg"));
    m_imageEdit->setReadOnly(true);

    pathForm->addRow(QStringLiteral("Размер:"), m_sizeEdit);
    pathForm->addRow(QStringLiteral("CRC:"), m_crcEdit);
    pathForm->addRow(QStringLiteral("Путь (part):"), m_partEdit);
    pathForm->addRow(QStringLiteral("Обложка:"), m_imageEdit);

    auto *descBox = new QGroupBox(QStringLiteral("Описание"));
    auto *descLayout = new QVBoxLayout(descBox);
    m_descEdit = new QPlainTextEdit;
    m_descEdit->setPlaceholderText(QStringLiteral("Описание книги (может быть пустым)"));
    m_descEdit->setMaximumHeight(120);
    descLayout->addWidget(m_descEdit);

    outer->addWidget(fileBox);
    outer->addWidget(mainBox);
    outer->addWidget(langBox);
    outer->addWidget(pathBox);
    outer->addWidget(descBox);
    outer->addStretch(1);
    return scrollHost;
}

void AddBookDialog::browseBook()
{
    const QString filter = buildDialogFilter(m_fileFilters);
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Выберите файл книги"), QString(), filter);
    if (file.isEmpty())
        return;
    m_selectedFile = file;
    m_fileEdit->setText(file);
    processSelectedFile();
}

void AddBookDialog::browseCover()
{
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Выберите обложку"), QString(),
        QStringLiteral("Изображения (*.jpg *.jpeg *.png *.bmp *.gif);;Все файлы (*.*)"));
    if (file.isEmpty())
        return;
    m_selectedCover = file;
    m_coverFileEdit->setText(file);
}

void AddBookDialog::processSelectedFile()
{
    m_md5 = md5OfFile(m_selectedFile);
    m_crcEdit->setText(m_md5);
    m_sizeEdit->setText(bytesToSizeText(QFileInfo(m_selectedFile).size()));

    const QString suffix = QFileInfo(m_selectedFile).suffix();
    if (!suffix.isEmpty())
        m_formatCombo->setCurrentText(QLatin1Char('.') + suffix);

    if (m_md5.isEmpty())
        return;

    QSqlQuery check(QSqlDatabase::database(QStringLiteral("book")));
    check.prepare(QStringLiteral("SELECT [index], name_book, ahtor FROM book "
                                 "WHERE crc = ?"));
    check.addBindValue(m_md5);
    if (!check.exec() || !check.next())
        return;

    const int id = check.value(0).toInt();
    const QString name = check.value(1).toString();
    const QString author = check.value(2).toString();
    QString msg = QStringLiteral("Книга уже есть в базе (ID %1)").arg(id);
    if (!name.isEmpty())
        msg += QStringLiteral("\nНазвание: %1").arg(name);
    if (!author.isEmpty())
        msg += QStringLiteral("\nАвтор: %1").arg(author);
    msg += QStringLiteral("\n\nУдалить выбранный файл с диска?");

    if (QMessageBox::question(this, QStringLiteral("Дубликат"), msg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        QFile::remove(m_selectedFile);
    }

    m_selectedFile.clear();
    m_fileEdit->clear();
    m_md5.clear();
    m_crcEdit->clear();
    m_sizeEdit->clear();
}

QString AddBookDialog::md5OfFile(const QString &path) const
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

int AddBookDialog::langCode() const
{
    int code = 0;
    for (int i = 0; i < m_langChecks.size() && i < kLangCount; ++i) {
        if (m_langChecks.at(i)->isChecked())
            code += kLangTable[i].code;
    }
    return code;
}

QString AddBookDialog::langString() const
{
    QStringList names;
    for (int i = 0; i < m_langChecks.size() && i < kLangCount; ++i) {
        if (m_langChecks.at(i)->isChecked())
            names << QString::fromUtf8(kLangTable[i].stored);
    }
    return names.join(QStringLiteral(", "));
}

QString AddBookDialog::commit()
{
    if (m_selectedFile.isEmpty())
        return QStringLiteral("Не выбран файл книги.");

    const QString part = m_partEdit->text().trimmed();
    const QString relPart = part.isEmpty()
        ? QString()
        : (part.endsWith(QLatin1Char('/')) || part.endsWith(QLatin1Char('\\'))
               ? part
               : part + QLatin1Char('/'));

    int nextIndex = 1;
    {
        QSqlQuery maxQuery(QSqlDatabase::database(QStringLiteral("book")));
        if (maxQuery.exec(QStringLiteral("SELECT MAX([index]) + 1 FROM book")) &&
            maxQuery.next())
            nextIndex = maxQuery.value(0).toInt();
    }

    const QString folderRel = QStringLiteral("book\\%1\\").arg(nextIndex);
    const QDir targetDir(QDir(m_baseDir).absoluteFilePath(folderRel));
    if (!QDir().mkpath(targetDir.absolutePath()))
        return QStringLiteral("Не удалось создать папку: %1")
            .arg(targetDir.absolutePath());

    const QString fileName = m_authorEdit->text().trimmed() +
                             m_formatCombo->currentText().trimmed();
    const QString destFile = targetDir.filePath(fileName);
    if (!QFile::copy(m_selectedFile, destFile))
        return QStringLiteral("Не удалось скопировать файл: %1").arg(destFile);

    QString coverSrc = m_selectedCover;
    if (coverSrc.isEmpty())
        coverSrc = m_noImagePath;
    const QString coverDest = targetDir.filePath(QStringLiteral("cover.jpg"));
    if (!coverSrc.isEmpty() && QFileInfo::exists(coverSrc))
        QFile::copy(coverSrc, coverDest);

    QFile desc(targetDir.filePath(QStringLiteral("descripter.txt")));
    if (desc.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        desc.write(m_descEdit->toPlainText().toUtf8());
        desc.close();
    }

    QFile::remove(m_selectedFile);

    QSqlQuery query(QSqlDatabase::database(QStringLiteral("book")));
    query.prepare(QStringLiteral(
        "INSERT INTO book ([index], name_book, ahtor, yaer, format, lang, lang_string, "
        "langav, tematica, system, studio, vid, source, pages, izdatel, size, crc, "
        "part, image) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(nextIndex);
    query.addBindValue(m_nameEdit->text().trimmed());
    query.addBindValue(m_authorEdit->text().trimmed());
    query.addBindValue(m_yearSpin->value() == 0 ? QVariant() : QVariant(m_yearSpin->value()));
    query.addBindValue(m_formatCombo->currentText().trimmed());
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
    query.addBindValue(m_sizeEdit->text().trimmed());
    query.addBindValue(m_md5);
    query.addBindValue(QStringLiteral("book\\%1\\").arg(nextIndex));
    query.addBindValue(m_imageEdit->text().trimmed());
    if (!query.exec())
        return QStringLiteral("Не удалось добавить книгу:\n%1").arg(query.lastError().text());

    return QString();
}
