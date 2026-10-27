#pragma once

#include <QString>

namespace DbSchema {

// Создаёт недостающие таблицы: crc_history, archive_book
bool ensureTables(QString *error = nullptr);

// Добавляет в book недостающие колонки (для старых баз)
bool ensureBookColumns(QString *error = nullptr);

// Известен ли CRC книге (в book или в истории)? excludeId — сама книга.
bool crcKnown(const QString &crc, int excludeId = 0);

// Запоминает CRC в историю (чтобы перевод книги не проходил как «новая»)
void rememberCrc(const QString &crc, int bookId, const QString &note = QString());

// Переносит книгу в архив. removeFile=true — удалить файл книги с диска.
bool archiveBook(const QString &baseDir, int bookId, bool removeFile,
                 const QString &note, QString *error);

} // namespace DbSchema
