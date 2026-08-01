#ifndef CONVERTER_H
#define CONVERTER_H

#include "mdblib.h"

#include <QObject>
#include <QSqlDatabase>

class Converter : public QObject
{
    Q_OBJECT
public:
    explicit Converter(QObject *parent = nullptr);

public slots:
    void convert(const QString &inPath, const QString &outPath);

signals:
    void logMessage(const QString &text);
    void errorMessage(const QString &text);
    void progressChanged(int done, int total);
    void finished(bool ok, const QString &message);

private:
    bool createSqliteSchema(const mdb::TableDef &table, QSqlDatabase &db);
    bool importTable(const mdb::MdbFile &file, const mdb::TableDef &table, QSqlDatabase &db);
};

#endif // CONVERTER_H
