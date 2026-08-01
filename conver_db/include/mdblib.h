#ifndef MDB_LIB_H
#define MDB_LIB_H

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QVariant>
#include <QStringList>

namespace mdb {

enum JetVersion {
    JetUnknown = 0,
    Jet3 = 3,
    Jet4 = 4,
};

enum ColType {
    COL_BOOL    = 0x01,
    COL_BYTE    = 0x02,
    COL_INT     = 0x03,
    COL_LONGINT = 0x04,
    COL_MONEY   = 0x05,
    COL_FLOAT   = 0x06,
    COL_DOUBLE  = 0x07,
    COL_DATETIME= 0x08,
    COL_BINARY  = 0x09,
    COL_TEXT    = 0x0A,
    COL_OLE     = 0x0B,
    COL_MEMO    = 0x0C,
    COL_REPID   = 0x0F,
    COL_NUMERIC = 0x10,
};

struct Column {
    int      colType = 0;       /* type byte */
    int      colNum  = 0;       /* column number (includes deleted), used for null mask */
    int      varColNum = 0;     /* index into variable column offset table */
    int      rowColNum = 0;
    int      flags   = 0;
    int      fixedOffset = 0;
    int      colSize = 0;
    QString  name;

    bool isFixed() const { return (flags & 0x01) != 0; }
    static QString typeName(int t);
};

struct TableDef {
    int         page = 0;
    qint64      numRows = 0;
    int         numCols = 0;
    int         numVarCols = 0;
    QString     name;
    QVector<Column> columns;    /* sorted by colNum */
};

struct CatalogEntry {
    QString name;
    int     type = 0;           /* low 7 bits: object type */
    quint32 flags = 0;
    int     tablePg = 0;        /* page of table definition */
};

struct FormatConstants {
    JetVersion jetVersion = JetUnknown;
    int  pgSize = 0;
    int  rowCountOffset = 0;
    int  tabNumRowsOffset = 0;
    int  tabNumColsOffset = 0;
    int  tabNumIdxsOffset = 0;
    int  tabNumRidxsOffset = 0;
    int  tabUsageMapOffset = 0;
    int  tabFirstDpgOffset = 0;
    int  tabColsStartOffset = 0;
    int  tabRidxEntrySize = 0;
    int  colScaleOffset = 0;
    int  colPrecOffset = 0;
    int  colFlagsOffset = 0;
    int  colSizeOffset = 0;
    int  colNumOffset = 0;
    int  tabColEntrySize = 0;
    int  tabFreeMapOffset = 0;
    int  tabColOffsetVar = 0;
    int  tabColOffsetFixed = 0;
    int  tabRowColNumOffset = 0;
    int  colCountSize = 0;      /* 1 for Jet3, 2 for Jet4 */
    int  defaultCodePage = 1252;
};

class MdbFile {
public:
    MdbFile();
    ~MdbFile();

    bool open(const QString &path);
    void close();

    bool isOpen() const { return !m_data.isEmpty(); }
    JetVersion version() const { return m_fmt.jetVersion; }
    int  pageSize() const { return m_fmt.pgSize; }
    int  numPages() const { return m_numPages; }
    const FormatConstants &fmt() const { return m_fmt; }
    FormatConstants &fmtW() { return m_fmt; }

    /* returns false if page number out of range */
    bool readPage(int pg, QByteArray &out) const;
    /* whole-file byte access helper */
    const char *ptr(qint64 offset, int len) const;
    QByteArray bytes(qint64 offset, int len) const;

    quint8  u8(qint64 off) const;
    quint16 u16(qint64 off) const;
    quint32 u32(qint64 off) const;
    qint32  i32(qint64 off) const;
    double  dbl(qint64 off) const;

private:
    QByteArray m_data;
    int m_numPages = 0;
    FormatConstants m_fmt;

    friend bool initFormat(MdbFile &file);
};

/* Read database definition page, set format constants */
bool initFormat(MdbFile &file);

/* Table definition reading */
bool readTableDef(MdbFile &file, const CatalogEntry &entry, TableDef &out);

/* Data reading.
   Reads all data pages of the table (using usage map when possible,
   otherwise brute-force scan). For each row, decode values into a
   QVector<QVariant> aligned with table->columns. */
QVector<int> tableDataPages(const MdbFile &file, const TableDef &table);
bool readRowValues(const MdbFile &file, int page, int row, const TableDef &table,
                   QVector<QVariant> &out);
/* read raw bytes of a row on a page */
QByteArray readRowBytes(const MdbFile &file, int page, int row);

/* Catalog (MSysObjects) reading */
bool readCatalog(MdbFile &file, QVector<CatalogEntry> &out);

/* Helpers */
QString jetVersionString(JetVersion v);
QString valueToString(const QVariant &v);

} // namespace mdb

#endif
