#include "massadddialog.h"
#include "settings.h"

#include <QCheckBox>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

const QSet<QString> kBookExts = {
    QStringLiteral("pdf"),  QStringLiteral("djvu"), QStringLiteral("djv"),
    QStringLiteral("fb2"),  QStringLiteral("epub"), QStringLiteral("chm"),
    QStringLiteral("html"), QStringLiteral("htm"),  QStringLiteral("mth"),
    QStringLiteral("txt"),  QStringLiteral("doc"),  QStringLiteral("docx"),
};
const QSet<QString> kArchiveExts = {
    QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"),
};
const QSet<QString> kImageExts = {
    QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("bmp"),  QStringLiteral("gif"),
};
const QSet<QString> kSkipFiles = {
    QStringLiteral("cover.jpg"),  QStringLiteral("thumbs.db"),
    QStringLiteral("descripter.txt"),
};

enum { ColCheck, ColFile, ColName, ColFormat, ColSize, ColStatus, ColCover,
       ColFilePath, ColCoverPath, ColCount };

QString bytesToSizeText(qint64 bytes)
{
    return QStringLiteral("%1 кб").arg(qRound64(bytes / 1024.0));
}

QStringList existingCrcs()
{
    QStringList crcs;
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("book")));
    if (q.exec(QStringLiteral("SELECT crc FROM book WHERE crc IS NOT NULL AND crc != ''")))
        while (q.next())
            crcs << q.value(0).toString().toUpper();
    return crcs;
}

int findNextIndex()
{
    QSet<int> used;
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("book")));
    if (q.exec(QStringLiteral("SELECT [index] FROM book")))
        while (q.next())
            used << q.value(0).toInt();
    int n = 0;
    while (used.contains(++n)) ;
    return n;
}

} // namespace

MassAddDialog::MassAddDialog(const QString &baseDir, QWidget *parent)
    : QDialog(parent)
    , m_baseDir(baseDir)
{
    setWindowTitle(QStringLiteral("Массовое добавление"));
    resize(900, 600);
    setMaximumHeight(SettingsManager::instance().maxFormHeight());

    auto *layout = new QVBoxLayout(this);

    auto *topRow = new QHBoxLayout;
    m_folderEdit = new QLineEdit;
    m_folderEdit->setPlaceholderText(QStringLiteral("Выберите папку с книгами..."));
    auto *browseBtn = new QPushButton(QStringLiteral("Обзор..."));
    connect(browseBtn, &QPushButton::clicked, this, &MassAddDialog::browseFolder);
    auto *scanBtn = new QPushButton(QStringLiteral("Сканировать"));
    connect(scanBtn, &QPushButton::clicked, this, &MassAddDialog::scanFolder);
    topRow->addWidget(m_folderEdit, 1);
    topRow->addWidget(browseBtn);
    topRow->addWidget(scanBtn);
    layout->addLayout(topRow);

    m_table = new QTableWidget;
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels(
        {QStringLiteral(""),     QStringLiteral("Файл"),
         QStringLiteral("Название"), QStringLiteral("Формат"),
         QStringLiteral("Размер"), QStringLiteral("Статус"),
         QStringLiteral("Обложка"),
         QStringLiteral("Путь"),  QStringLiteral("ПутьОбложки")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(ColCheck, QHeaderView::Fixed);
    m_table->setColumnWidth(ColCheck, 30);
    m_table->setColumnHidden(ColFilePath, true);
    m_table->setColumnHidden(ColCoverPath, true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *bottomRow = new QHBoxLayout;
    m_statusLabel = new QLabel;
    bottomRow->addWidget(m_statusLabel, 1);
    m_toggleButton = new QPushButton(QStringLiteral("Выбрать все"));
    connect(m_toggleButton, &QPushButton::clicked, this, &MassAddDialog::toggleAll);
    bottomRow->addWidget(m_toggleButton);
    m_addButton = new QPushButton(QStringLiteral("Добавить выбранные"));
    m_addButton->setEnabled(false);
    connect(m_addButton, &QPushButton::clicked, this, &MassAddDialog::addSelected);
    bottomRow->addWidget(m_addButton);
    layout->addLayout(bottomRow);
}

void MassAddDialog::browseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Выберите папку с книгами"), m_folderEdit->text());
    if (!dir.isEmpty())
        m_folderEdit->setText(dir);
}

void MassAddDialog::scanFolder()
{
    const QString dir = m_folderEdit->text().trimmed();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Укажите существующую папку."));
        return;
    }

    m_table->setRowCount(0);
    m_newCount = 0;
    m_dupCount = 0;
    m_addButton->setEnabled(false);

    const QStringList crcs = existingCrcs();
    QList<ScannedBook> books;
    scanRecursive(dir, &books);

    int deletedCount = 0;

    for (auto it = books.begin(); it != books.end(); ) {
        auto &b = *it;
        b.crc = computeCrc(b.filePath);
        if (crcs.contains(b.crc)) {
            b.duplicate = true;
            ++m_dupCount;

            QFile::remove(b.filePath);
            if (!b.coverPath.isEmpty())
                QFile::remove(b.coverPath);
            if (!b.zipPath.isEmpty())
                QFile::remove(b.zipPath);
            ++deletedCount;

            it = books.erase(it);
        } else {
            b.duplicate = false;
            ++m_newCount;
            ++it;
        }
    }

    populateTable(books);
    m_addButton->setEnabled(m_newCount > 0);

    QString status = QStringLiteral("Найдено: %1 новых, %2 дубликатов удалено")
                         .arg(m_newCount)
                         .arg(deletedCount);
    m_statusLabel->setText(status);
}

void MassAddDialog::scanRecursive(const QString &dir, QList<ScannedBook> *books)
{
    QDirIterator it(dir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    QHash<QString, QStringList> dirFiles;

    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isDir())
            continue;
        if (kSkipFiles.contains(it.fileName().toLower()))
            continue;
        dirFiles[it.path()] << it.filePath();
    }

    QHashIterator<QString, QStringList> dit(dirFiles);
    while (dit.hasNext()) {
        dit.next();
        const QStringList &files = dit.value();

        QStringList bookFiles, archiveFiles, imageFiles;
        for (const QString &f : files) {
            const QString ext = QFileInfo(f).suffix().toLower();
            if (kBookExts.contains(ext))
                bookFiles << f;
            else if (kArchiveExts.contains(ext))
                archiveFiles << f;
            else if (kImageExts.contains(ext))
                imageFiles << f;
        }

        for (const QString &bf : bookFiles) {
            ScannedBook book;
            book.filePath = bf;
            book.fileName = QFileInfo(bf).fileName();
            book.nameBook = QFileInfo(bf).completeBaseName();
            book.format = QStringLiteral(".") + QFileInfo(bf).suffix().toLower();
            book.sizeText = bytesToSizeText(QFileInfo(bf).size());

            const QString bookBase = QFileInfo(bf).completeBaseName().toLower();
            for (const QString &af : archiveFiles) {
                if (QFileInfo(af).completeBaseName().toLower() == bookBase) {
                    book.zipPath = af;
                    break;
                }
            }

            if (bookFiles.size() == 1 && imageFiles.size() == 1)
                book.coverPath = imageFiles.first();

            books->append(book);
        }
    }
}

QString MassAddDialog::computeCrc(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash hash(QCryptographicHash::Md5);
    QByteArray buffer(64 * 1024, Qt::Uninitialized);
    while (!file.atEnd()) {
        const qint64 n = file.read(buffer.data(), buffer.size());
        if (n <= 0) break;
        hash.addData(buffer.constData(), n);
    }
    return QString::fromLatin1(hash.result().toHex()).toUpper();
}

void MassAddDialog::populateTable(const QList<ScannedBook> &books)
{
    m_table->setRowCount(books.size());
    for (int i = 0; i < books.size(); ++i) {
        const auto &b = books.at(i);

        auto *cb = new QCheckBox;
        cb->setChecked(!b.duplicate);
        cb->setEnabled(!b.duplicate);
        m_table->setCellWidget(i, ColCheck, cb);

        auto makeItem = [](const QString &text) {
            return new QTableWidgetItem(text);
        };

        m_table->setItem(i, ColFile, makeItem(b.fileName));
        m_table->setItem(i, ColName, makeItem(b.nameBook));
        m_table->setItem(i, ColFormat, makeItem(b.format));
        m_table->setItem(i, ColSize, makeItem(b.sizeText));

        auto *statusItem = makeItem(b.duplicate
                                        ? QStringLiteral("Есть в базе")
                                        : QStringLiteral("Новая"));
        statusItem->setForeground(b.duplicate ? Qt::gray : Qt::darkGreen);
        m_table->setItem(i, ColStatus, statusItem);

        m_table->setItem(i, ColCover,
                         makeItem(b.coverPath.isEmpty()
                                      ? QStringLiteral("—")
                                      : QFileInfo(b.coverPath).fileName()));
        m_table->setItem(i, ColFilePath, makeItem(b.filePath));
        m_table->setItem(i, ColCoverPath, makeItem(b.coverPath));
    }
    m_table->horizontalHeader()->setSectionResizeMode(ColFile, QHeaderView::Stretch);
}

void MassAddDialog::toggleAll()
{
    bool allChecked = true;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto *cb = qobject_cast<QCheckBox *>(m_table->cellWidget(i, ColCheck));
        if (cb && cb->isEnabled() && !cb->isChecked()) {
            allChecked = false;
            break;
        }
    }
    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto *cb = qobject_cast<QCheckBox *>(m_table->cellWidget(i, ColCheck));
        if (cb && cb->isEnabled())
            cb->setChecked(!allChecked);
    }
}

void MassAddDialog::addSelected()
{
    int count = 0;
    QStringList errors;

    for (int i = 0; i < m_table->rowCount(); ++i) {
        auto *cb = qobject_cast<QCheckBox *>(m_table->cellWidget(i, ColCheck));
        if (!cb || !cb->isChecked())
            continue;

        const QString filePath   = m_table->item(i, ColFilePath)->text();
        const QString coverSrc   = m_table->item(i, ColCoverPath)->text();
        const QString nameBook   = m_table->item(i, ColName)->text();
        const QString format     = m_table->item(i, ColFormat)->text();
        const QString sizeText   = m_table->item(i, ColSize)->text();

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            errors << QStringLiteral("Не удалось открыть: %1").arg(filePath);
            continue;
        }
        QCryptographicHash hash(QCryptographicHash::Md5);
        QByteArray buf(64 * 1024, Qt::Uninitialized);
        while (!file.atEnd()) {
            const qint64 n = file.read(buf.data(), buf.size());
            if (n <= 0) break;
            hash.addData(buf.constData(), n);
        }
        file.close();
        const QString crc = QString::fromLatin1(hash.result().toHex()).toUpper();

        const int newId = findNextIndex();
        const QString folderRel = QStringLiteral("book\\%1\\").arg(newId);
        const QString targetDir = QDir(m_baseDir).absoluteFilePath(folderRel);
        if (!QDir().mkpath(targetDir)) {
            errors << QStringLiteral("Не удалось создать: %1").arg(targetDir);
            continue;
        }

        const QString destFile = QDir(targetDir).filePath(
            QFileInfo(filePath).fileName());
        if (!QFile::copy(filePath, destFile)) {
            errors << QStringLiteral("Не удалось скопировать: %1").arg(destFile);
            continue;
        }

        for (int row = 0; row < m_table->rowCount(); ++row) {
            if (row == i) continue;
            auto *otherCb = qobject_cast<QCheckBox *>(m_table->cellWidget(row, ColCheck));
            if (!otherCb || !otherCb->isEnabled()) continue;
            if (m_table->item(row, ColName)->text() == nameBook &&
                m_table->item(row, ColFormat)->text() == format) {
                QFile::remove(m_table->item(row, ColFilePath)->text());
                otherCb->setEnabled(false);
                m_table->item(row, ColStatus)->setText(QStringLiteral("Добавлено (dup)"));
                m_table->item(row, ColStatus)->setForeground(Qt::gray);
            }
        }

        QSqlQuery q(QSqlDatabase::database(QStringLiteral("book")));
        q.prepare(QStringLiteral(
            "INSERT INTO book ([index], name_book, ahtor, format, size, crc, "
            "part, image) VALUES (?, ?, ?, ?, ?, ?, ?, 'cover.jpg')"));
        q.addBindValue(newId);
        q.addBindValue(nameBook);
        q.addBindValue(QString());
        q.addBindValue(format);
        q.addBindValue(sizeText);
        q.addBindValue(crc);
        q.addBindValue(folderRel);
        if (!q.exec()) {
            errors << QStringLiteral("Ошибка БД (ID %1): %2")
                          .arg(newId)
                          .arg(q.lastError().text());
            continue;
        }

        if (!coverSrc.isEmpty() && coverSrc != QStringLiteral("—")) {
            if (QFileInfo::exists(coverSrc)) {
                QFile::copy(coverSrc, QDir(targetDir).filePath(QStringLiteral("cover.jpg")));
                QFile::remove(coverSrc);
            }
        }

        QFile::remove(filePath);

        cb->setEnabled(false);
        m_table->item(i, ColStatus)->setText(QStringLiteral("Добавлено"));
        m_table->item(i, ColStatus)->setForeground(Qt::blue);
        ++count;
    }

    if (!errors.isEmpty())
        QMessageBox::warning(this, QStringLiteral("Ошибки"),
                             errors.join(QStringLiteral("\n")));
    m_statusLabel->setText(QStringLiteral("Добавлено: %1 книг").arg(count));
    m_addButton->setEnabled(false);
}
