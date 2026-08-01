#include "converter.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QtEndian>

Converter::Converter(QObject *parent)
    : QObject(parent)
{
}

bool Converter::createSqliteSchema(const mdb::TableDef &table, QSqlDatabase &db)
{
    QSqlQuery q(db);
    QStringList colDefs;
    for (int i = 0; i < table.columns.size(); ++i) {
        const mdb::Column &c = table.columns[i];
        QString type;
        switch (c.colType) {
        case mdb::COL_BOOL:
        case mdb::COL_BYTE:
        case mdb::COL_INT:
        case mdb::COL_LONGINT:
            type = QStringLiteral("INTEGER");
            break;
        case mdb::COL_MONEY:
        case mdb::COL_FLOAT:
        case mdb::COL_DOUBLE:
            type = QStringLiteral("REAL");
            break;
        case mdb::COL_DATETIME:
            type = QStringLiteral("TEXT");
            break;
        case mdb::COL_BINARY:
        case mdb::COL_OLE:
        case mdb::COL_REPID:
        case mdb::COL_NUMERIC:
            type = QStringLiteral("BLOB");
            break;
        case mdb::COL_MEMO:
        case mdb::COL_TEXT:
        default:
            type = QStringLiteral("TEXT");
            break;
        }
        colDefs << QStringLiteral("\"%1\" %2").arg(c.name).arg(type);
    }
    const QString ddl = QStringLiteral("CREATE TABLE IF NOT EXISTS \"book\" (%1)")
                            .arg(colDefs.join(QStringLiteral(", ")));
    if (!q.exec(ddl)) {
        emit errorMessage(QStringLiteral("Ошибка создания таблицы: %1").arg(q.lastError().text()));
        return false;
    }
    return true;
}

bool Converter::importTable(const mdb::MdbFile &file, const mdb::TableDef &table, QSqlDatabase &db)
{
    const QVector<int> pages = mdb::tableDataPages(file, table);
    emit logMessage(QStringLiteral("Таблица \"%1\": %2 строк, %3 колонок, %4 страниц данных")
                        .arg(table.name)
                        .arg(table.numRows)
                        .arg(table.columns.size())
                        .arg(pages.size()));

    if (!createSqliteSchema(table, db))
        return false;

    QSqlQuery q(db);
    QStringList ph;
    for (int i = 0; i < table.columns.size(); ++i)
        ph << QStringLiteral("?");
    const QString ins = QStringLiteral("INSERT INTO \"book\" VALUES (%1)")
                            .arg(ph.join(QStringLiteral(", ")));
    if (!q.prepare(ins)) {
        emit errorMessage(QStringLiteral("Ошибка подготовки INSERT: %1").arg(q.lastError().text()));
        return false;
    }

    db.transaction();
    qint64 written = 0;
    int pageIndex = 0;
    for (int pg : pages) {
        QByteArray pgb;
        if (!file.readPage(pg, pgb))
            continue;
        const int nrows = (int)qFromLittleEndian<quint16>(
            (const uchar*)pgb.constData() + file.fmt().rowCountOffset);
        for (int r = 0; r < nrows; ++r) {
            QVector<QVariant> vals;
            if (!mdb::readRowValues(file, pg, r, table, vals))
                continue;
            for (const QVariant &v : vals)
                q.addBindValue(v);
            if (!q.exec()) {
                emit errorMessage(QStringLiteral("Ошибка вставки строки: %1").arg(q.lastError().text()));
                db.rollback();
                return false;
            }
            ++written;
        }
        emit progressChanged(++pageIndex, pages.size());
    }
    db.commit();
    emit logMessage(QStringLiteral("Импортировано строк: %1").arg(written));
    return true;
}

void Converter::convert(const QString &inPath, const QString &outPath)
{
    if (inPath.isEmpty() || outPath.isEmpty()) {
        emit errorMessage(QStringLiteral("Укажите входной и выходной файл."));
        emit finished(false, QStringLiteral("Не указаны пути"));
        return;
    }

    mdb::MdbFile file;
    if (!file.open(inPath)) {
        emit errorMessage(QStringLiteral("Не удалось открыть файл: %1").arg(inPath));
        emit finished(false, QStringLiteral("Ошибка открытия файла"));
        return;
    }
    if (!mdb::initFormat(file)) {
        emit errorMessage(QStringLiteral("Не удалось определить формат Jet в файле."));
        emit finished(false, QStringLiteral("Неподдерживаемый формат базы"));
        return;
    }
    emit logMessage(QStringLiteral("Файл: %1, формат: %2, страниц: %3, размер страницы: %4")
                        .arg(inPath, mdb::jetVersionString(file.version()))
                        .arg(file.numPages())
                        .arg(file.pageSize()));

    QVector<mdb::CatalogEntry> catalog;
    if (!mdb::readCatalog(file, catalog)) {
        emit errorMessage(QStringLiteral("Ошибка чтения каталога."));
        emit finished(false, QStringLiteral("Ошибка чтения каталога"));
        return;
    }
    emit logMessage(QStringLiteral("Объектов в каталоге: %1").arg(catalog.size()));

    const mdb::CatalogEntry *bookEntry = nullptr;
    for (const mdb::CatalogEntry &e : catalog) {
        if (e.type == 1 && e.name.compare(QStringLiteral("book"), Qt::CaseInsensitive) == 0) {
            bookEntry = &e;
            break;
        }
    }
    if (!bookEntry) {
        emit errorMessage(QStringLiteral("Таблица \"book\" не найдена."));
        emit finished(false, QStringLiteral("Таблица \"book\" не найдена"));
        return;
    }

    mdb::TableDef table;
    if (!mdb::readTableDef(file, *bookEntry, table)) {
        emit errorMessage(QStringLiteral("Ошибка чтения определения таблицы book."));
        emit finished(false, QStringLiteral("Ошибка чтения определения таблицы"));
        return;
    }

    if (QFileInfo::exists(outPath))
        QFile::remove(outPath);

    const QString connName = QStringLiteral("conv-%1")
                                 .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
    db.setDatabaseName(outPath);

    bool ok = false;
    if (!db.open()) {
        emit errorMessage(QStringLiteral("Не удалось открыть SQLite: %1").arg(db.lastError().text()));
    } else {
        ok = importTable(file, table, db);
        db.close();
    }
    QSqlDatabase::removeDatabase(connName);

    emit progressChanged(0, 0);
    emit finished(ok, ok ? QStringLiteral("Конвертация завершена успешно")
                         : QStringLiteral("Ошибка конвертации"));
}
