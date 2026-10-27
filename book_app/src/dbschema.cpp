#include "dbschema.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPair>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QVector>

namespace {

QSqlDatabase bookDb()
{
    return QSqlDatabase::database(QStringLiteral("book"));
}

} // namespace

namespace DbSchema {

bool ensureTables(QString *error)
{
    if (!QSqlDatabase::contains(QStringLiteral("book"))) {
        if (error)
            *error = QStringLiteral("БД не открыта.");
        return false;
    }

    QSqlQuery q(bookDb());

    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS crc_history ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " crc TEXT NOT NULL,"
            " book_id INTEGER,"
            " note TEXT,"
            " created TEXT)"))) {
        if (error)
            *error = QStringLiteral("crc_history: %1").arg(q.lastError().text());
        return false;
    }

    q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_crc_history_crc ON crc_history(crc)"));

    // Полная копия строки книги + когда/почему убрали в архив.
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS archive_book ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " book_index INTEGER,"
            " name_book TEXT, ahtor TEXT, yaer INTEGER, format TEXT,"
            " lang INTEGER, lang_string TEXT, langav TEXT,"
            " tematica TEXT, system TEXT, studio TEXT, vid TEXT,"
            " source TEXT, source_format TEXT,"
            " pages INTEGER, izdatel TEXT, part TEXT, image TEXT,"
            " size TEXT, crc TEXT, perevod TEXT, gruppa TEXT, quality TEXT, rang INTEGER,"
            " archive_date TEXT, archive_note TEXT)"))) {
        if (error)
            *error = QStringLiteral("archive_book: %1").arg(q.lastError().text());
        return false;
    }

    q.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_archive_name ON archive_book(name_book, ahtor)"));

    // Старые базы могли остаться без нужных колонок — добавляем на лету.
    ensureBookColumns(error);
    return true;
}

bool ensureBookColumns(QString *error)
{
    QSqlQuery check(bookDb());
    if (!check.exec(QStringLiteral("PRAGMA table_info(book)"))) {
        if (error)
            *error = QStringLiteral("PRAGMA table_info(book): %1")
                         .arg(check.lastError().text());
        return false;
    }

    QSet<QString> existing;
    while (check.next())
        existing << check.value(1).toString().toLower();

    if (existing.isEmpty()) {
        if (error)
            *error = QStringLiteral("В базе нет таблицы book.");
        return false;
    }

    // Колонки, на которые опирается приложение. SQLite не умеет DROP COLUMN,
    // поэтому недостающие просто добавляем через ALTER TABLE.
    static const QVector<QPair<QString, QString>> wanted = {
        {QStringLiteral("name_book"),  QStringLiteral("TEXT")},
        {QStringLiteral("ahtor"),      QStringLiteral("TEXT")},
        {QStringLiteral("yaer"),       QStringLiteral("INTEGER")},
        {QStringLiteral("format"),     QStringLiteral("TEXT")},
        {QStringLiteral("lang"),       QStringLiteral("INTEGER")},
        {QStringLiteral("lang_string"), QStringLiteral("TEXT")},
        {QStringLiteral("langav"),     QStringLiteral("TEXT")},
        {QStringLiteral("tematica"),   QStringLiteral("TEXT")},
        {QStringLiteral("system"),     QStringLiteral("TEXT")},
        {QStringLiteral("studio"),     QStringLiteral("TEXT")},
        {QStringLiteral("vid"),        QStringLiteral("TEXT")},
        {QStringLiteral("source"),     QStringLiteral("TEXT")},
        {QStringLiteral("source_format"), QStringLiteral("TEXT")},
        {QStringLiteral("pages"),      QStringLiteral("INTEGER")},
        {QStringLiteral("izdatel"),    QStringLiteral("TEXT")},
        {QStringLiteral("part"),       QStringLiteral("TEXT")},
        {QStringLiteral("image"),      QStringLiteral("TEXT")},
        {QStringLiteral("size"),       QStringLiteral("TEXT")},
        {QStringLiteral("crc"),        QStringLiteral("TEXT")},
        {QStringLiteral("perevod"),    QStringLiteral("TEXT")},
        {QStringLiteral("gruppa"),     QStringLiteral("TEXT")},
        {QStringLiteral("quality"),    QStringLiteral("TEXT")},
        {QStringLiteral("rang"),       QStringLiteral("INTEGER")},
    };

    QStringList added;
    QSqlQuery alter(bookDb());
    for (const auto &pair : wanted) {
        if (existing.contains(pair.first))
            continue;
        if (alter.exec(QStringLiteral("ALTER TABLE book ADD COLUMN %1 %2")
                           .arg(pair.first, pair.second))) {
            added << pair.first;
        } else if (error) {
            *error = QStringLiteral("Не удалось добавить колонку %1: %2")
                         .arg(pair.first, alter.lastError().text());
        }
    }

    // Индекс для быстрых выборок по CRC (нужен и дублям, и истории).
    alter.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_book_crc ON book(crc)"));

    if (!added.isEmpty() && error)
        *error = QStringLiteral("Добавлены колонки: %1").arg(added.join(QStringLiteral(", ")));
    return true;
}

bool crcKnown(const QString &crc, int excludeId)
{
    const QString value = crc.trimmed().toUpper();
    if (value.isEmpty())
        return false;

    QSqlQuery q(bookDb());
    q.prepare(QStringLiteral("SELECT 1 FROM book WHERE UPPER(crc) = ? AND [index] != ? LIMIT 1"));
    q.addBindValue(value);
    q.addBindValue(excludeId);
    if (q.exec() && q.next())
        return true;

    QSqlQuery h(bookDb());
    h.prepare(QStringLiteral(
        "SELECT 1 FROM crc_history WHERE UPPER(crc) = ? AND IFNULL(book_id, 0) != ? LIMIT 1"));
    h.addBindValue(value);
    h.addBindValue(excludeId);
    return h.exec() && h.next();
}

void rememberCrc(const QString &crc, int bookId, const QString &note)
{
    const QString value = crc.trimmed().toUpper();
    if (value.isEmpty())
        return;

    QSqlQuery dup(bookDb());
    dup.prepare(QStringLiteral(
        "SELECT 1 FROM crc_history WHERE UPPER(crc) = ? AND IFNULL(book_id, 0) = ? LIMIT 1"));
    dup.addBindValue(value);
    dup.addBindValue(bookId);
    if (dup.exec() && dup.next())
        return;

    QSqlQuery q(bookDb());
    q.prepare(QStringLiteral(
        "INSERT INTO crc_history (crc, book_id, note, created) VALUES (?, ?, ?, ?)"));
    q.addBindValue(value);
    q.addBindValue(bookId);
    q.addBindValue(note);
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    q.exec();
}

bool archiveBook(const QString &baseDir, int bookId, bool removeFile,
                 const QString &note, QString *error)
{
    QSqlQuery read(bookDb());
    read.prepare(QStringLiteral(
        "SELECT name_book, ahtor, yaer, format, lang, lang_string, langav, "
        "tematica, system, studio, vid, source, source_format, pages, izdatel, "
        "part, image, size, crc, perevod, gruppa, quality, rang "
        "FROM book WHERE [index] = ?"));
    read.addBindValue(bookId);
    if (!read.exec() || !read.next()) {
        if (error)
            *error = QStringLiteral("Книга ID %1 не найдена.").arg(bookId);
        return false;
    }

    ensureTables();

    QSqlQuery ins(bookDb());
    ins.prepare(QStringLiteral(
        "INSERT INTO archive_book (book_index, name_book, ahtor, yaer, format, "
        "lang, lang_string, langav, tematica, system, studio, vid, source, "
        "source_format, pages, izdatel, part, image, size, crc, perevod, gruppa, "
        "quality, rang, archive_date, archive_note) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    ins.addBindValue(bookId);
    for (int i = 0; i < 23; ++i)
        ins.addBindValue(read.value(i));
    ins.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss")));
    ins.addBindValue(note);
    if (!ins.exec()) {
        if (error)
            *error = QStringLiteral("Не удалось записать в архив: %1").arg(ins.lastError().text());
        return false;
    }

    rememberCrc(read.value(18).toString(), bookId, QStringLiteral("архив"));

    if (removeFile) {
        const QString part = read.value(15).toString();
        QString rel = part.trimmed();
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        while (rel.endsWith(QLatin1Char('/')))
            rel.chop(1);
        while (rel.startsWith(QLatin1Char('/')))
            rel.remove(0, 1);

        // Удаляем каталог книги целиком: файл, обложку, описание, приложение.
        // Защита: путь должен быть строго внутри baseDir и содержать поддиректорию (book\ID).
        const QString base = QDir::cleanPath(QDir(baseDir).absolutePath());
        const QString dir = QDir::cleanPath(QDir(base).absoluteFilePath(rel));
        const bool inside = dir.startsWith(base + QLatin1Char('/'));
        const int depth = rel.count(QLatin1Char('/')) + 1;
        if (rel.isEmpty() || !inside || depth < 2) {
            if (error)
                *error = QStringLiteral("Небезопасный путь к каталогу книги, удаление пропущено: %1")
                             .arg(part);
        } else if (QDir(dir).exists()) {
            if (!QDir(dir).removeRecursively() && error)
                *error = QStringLiteral("Не удалось удалить каталог:\n%1").arg(dir);
        }
    }

    QSqlQuery del(bookDb());
    del.prepare(QStringLiteral("DELETE FROM book WHERE [index] = ?"));
    del.addBindValue(bookId);
    if (!del.exec()) {
        if (error)
            *error = QStringLiteral("Копия в архиве создана, но строку удалить не удалось: %1")
                         .arg(del.lastError().text());
        return false;
    }

    return true;
}

} // namespace DbSchema
