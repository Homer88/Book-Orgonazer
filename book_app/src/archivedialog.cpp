#include "archivedialog.h"
#include "dbschema.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
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

enum { ColId, ColName, ColAuthor, ColFormat, ColSize, ColCrc, ColDate, ColCount };

QString fmtSize(const QString &text)
{
    return text.isEmpty() ? QStringLiteral("—") : text;
}

} // namespace

ArchiveDialog::ArchiveDialog(const QString &baseDir, int bookId, QWidget *parent)
    : QDialog(parent)
    , m_baseDir(baseDir)
    , m_bookId(bookId)
{
    setWindowTitle(QStringLiteral("Перевести дубль в архив"));
    resize(820, 520);

    m_nameEdit = new QLineEdit;
    m_nameEdit->setReadOnly(true);
    m_authorEdit = new QLineEdit;
    m_authorEdit->setReadOnly(true);
    m_summary = new QLabel;

    QFormLayout *infoForm = new QFormLayout;
    infoForm->addRow(QStringLiteral("Книга (текущая):"), m_nameEdit);
    infoForm->addRow(QStringLiteral("Автор:"), m_authorEdit);

    m_table = new QTableWidget;
    m_table->setColumnCount(ColCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("Название"),
                                        QStringLiteral("Автор"), QStringLiteral("Формат"),
                                        QStringLiteral("Размер"), QStringLiteral("CRC"),
                                        QStringLiteral("Год")});
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setStretchLastSection(true);

    m_deleteFile = new QCheckBox(QStringLiteral("Удалить каталог книги с диска"));
    m_deleteFile->setChecked(true);
    m_deleteFile->setToolTip(QStringLiteral("Удаляется вся папка книги: файл, обложка, "
                                            "описание и приложение.\n"
                                            "Снимите галочку, чтобы оставить файлы."));
    m_noteEdit = new QLineEdit;
    m_noteEdit->setPlaceholderText(QStringLiteral("Например: перевод, новое издание"));

    m_archiveButton = new QPushButton(QStringLiteral("Перевести в архив"));
    connect(m_archiveButton, &QPushButton::clicked, this, &ArchiveDialog::archiveSelected);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QFormLayout *optForm = new QFormLayout;
    optForm->addRow(m_deleteFile);
    optForm->addRow(QStringLiteral("Примечание:"), m_noteEdit);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addWidget(m_archiveButton, 0);
    btnRow->addStretch(1);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->addLayout(infoForm);
    outer->addWidget(m_summary);
    outer->addWidget(m_table, 1);
    outer->addLayout(optForm);
    outer->addLayout(btnRow);
    outer->addWidget(box);

    QSqlQuery q(QSqlDatabase::database(QStringLiteral("book")));
    q.prepare(QStringLiteral("SELECT name_book, ahtor FROM book WHERE [index] = ?"));
    q.addBindValue(bookId);
    if (q.exec() && q.next()) {
        m_nameEdit->setText(q.value(0).toString());
        m_authorEdit->setText(q.value(1).toString());
    }

    findDuplicates();
}

void ArchiveDialog::findDuplicates()
{
    m_table->setRowCount(0);

    QSqlQuery self(QSqlDatabase::database(QStringLiteral("book")));
    self.prepare(QStringLiteral("SELECT name_book, ahtor, crc FROM book WHERE [index] = ?"));
    self.addBindValue(m_bookId);
    if (!self.exec() || !self.next()) {
        m_summary->setText(QStringLiteral("Книга не найдена в базе."));
        return;
    }
    const QString name = self.value(0).toString().trimmed();
    const QString author = self.value(1).toString().trimmed();
    const QString crc = self.value(2).toString().trimmed().toUpper();

    QSqlQuery q(QSqlDatabase::database(QStringLiteral("book")));
    q.prepare(QStringLiteral(
        "SELECT [index], name_book, ahtor, format, size, crc, yaer FROM book "
        "WHERE [index] != ? AND ("
        "  IFNULL(TRIM(name_book), '') = IFNULL(?, '') OR "
        "  IFNULL(TRIM(ahtor), '') = IFNULL(?, '')"
        ") ORDER BY [index]"));
    q.addBindValue(m_bookId);
    q.addBindValue(name);
    q.addBindValue(author);
    if (!q.exec()) {
        m_summary->setText(QStringLiteral("Ошибка поиска: %1").arg(q.lastError().text()));
        return;
    }

    int sameCrc = 0;
    int found = 0;
    while (q.next()) {
        const QString otherCrc = q.value(5).toString().trimmed().toUpper();
        if (!crc.isEmpty() && otherCrc == crc) {
            ++sameCrc;
            continue;
        }

        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, ColId, new QTableWidgetItem(q.value(0).toString()));
        m_table->setItem(row, ColName, new QTableWidgetItem(q.value(1).toString()));
        m_table->setItem(row, ColAuthor, new QTableWidgetItem(q.value(2).toString()));
        m_table->setItem(row, ColFormat, new QTableWidgetItem(q.value(3).toString()));
        m_table->setItem(row, ColSize, new QTableWidgetItem(fmtSize(q.value(4).toString())));
        m_table->setItem(row, ColCrc, new QTableWidgetItem(otherCrc));
        m_table->setItem(row, ColDate, new QTableWidgetItem(q.value(6).toString()));
        ++found;
    }

    if (m_table->rowCount() > 0)
        m_table->selectRow(0);

    m_archiveButton->setEnabled(found > 0);

    if (found == 0 && sameCrc == 0)
        m_summary->setText(QStringLiteral("Дублей не найдено."));
    else if (found == 0)
        m_summary->setText(QStringLiteral("Совпадений по названию/автору нет "
                                         "(%1 записей с тем же CRC — это тот же файл).")
                               .arg(sameCrc));
    else
        m_summary->setText(QStringLiteral("Найдено дублей с другим форматом/CRC: %1 "
                                         "(совпадений по CRC пропущено: %2)")
                               .arg(found).arg(sameCrc));
}

int ArchiveDialog::selectedId() const
{
    const int row = m_table->currentRow();
    if (row < 0 || !m_table->item(row, ColId))
        return 0;
    return m_table->item(row, ColId)->text().toInt();
}

void ArchiveDialog::archiveSelected()
{
    const int id = selectedId();
    if (id <= 0) {
        QMessageBox::information(this, QStringLiteral("Архив"),
                                 QStringLiteral("Выберите книгу из списка."));
        return;
    }

    const bool remove = m_deleteFile->isChecked();
    QString msg = remove
        ? QStringLiteral("Перенести книгу ID %1 в архив и УДАЛИТЬ её каталог с диска?\n\n"
                         "Будут удалены: файл книги, обложка, описание и приложение.\n"
                         "CRC останется в истории, поэтому книга больше не добавится повторно.")
              .arg(id)
        : QStringLiteral("Перенести книгу ID %1 в архив, оставив файлы на диске?").arg(id);
    if (QMessageBox::question(this, QStringLiteral("Перевести в архив"), msg,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes)
        != QMessageBox::Yes)
        return;

    QString error;
    if (!DbSchema::archiveBook(m_baseDir, id, remove, m_noteEdit->text().trimmed(), &error)) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"), error);
        return;
    }

    QMessageBox::information(this, QStringLiteral("Готово"),
                             QStringLiteral("Книга ID %1 перенесена в архив.").arg(id));
    findDuplicates();
}
