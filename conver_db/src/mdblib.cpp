#include "mdblib.h"

#include <QFile>
#include <QDateTime>
#include <QMetaType>
#include <QtEndian>
#include <algorithm>

namespace mdb {

QString Column::typeName(int t)
{
    switch (t) {
    case COL_BOOL:     return "BOOL";
    case COL_BYTE:     return "BYTE";
    case COL_INT:      return "INT";
    case COL_LONGINT:  return "LONGINT";
    case COL_MONEY:    return "MONEY";
    case COL_FLOAT:    return "FLOAT";
    case COL_DOUBLE:   return "DOUBLE";
    case COL_DATETIME: return "DATETIME";
    case COL_BINARY:   return "BINARY";
    case COL_TEXT:     return "TEXT";
    case COL_OLE:      return "OLE";
    case COL_MEMO:     return "MEMO";
    case COL_REPID:    return "REPID";
    case COL_NUMERIC:  return "NUMERIC";
    default:           return QString("UNKNOWN(0x%1)").arg(t, 2, 16, QLatin1Char('0'));
    }
}

MdbFile::MdbFile() = default;
MdbFile::~MdbFile() = default;

bool MdbFile::open(const QString &path)
{
    close();
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    m_data = f.readAll();
    f.close();
    if (m_data.size() < 128)
        return false;
    m_numPages = 0;
    return true;
}

void MdbFile::close()
{
    m_data.clear();
    m_numPages = 0;
    m_fmt = FormatConstants();
}

bool MdbFile::readPage(int pg, QByteArray &out) const
{
    if (pg < 0 || !m_fmt.pgSize)
        return false;
    const qint64 off = (qint64)pg * m_fmt.pgSize;
    if (off + m_fmt.pgSize > m_data.size())
        return false;
    out = m_data.mid(off, m_fmt.pgSize);
    return true;
}

const char *MdbFile::ptr(qint64 offset, int len) const
{
    if (offset < 0 || offset + len > m_data.size())
        return nullptr;
    return m_data.constData() + offset;
}

QByteArray MdbFile::bytes(qint64 offset, int len) const
{
    if (offset < 0 || offset + len > m_data.size())
        return QByteArray();
    return m_data.mid(offset, len);
}

quint8 MdbFile::u8(qint64 off) const
{
    const char *p = ptr(off, 1);
    return p ? (quint8)p[0] : 0;
}

quint16 MdbFile::u16(qint64 off) const
{
    const char *p = ptr(off, 2);
    return p ? qFromLittleEndian<quint16>(p) : 0;
}

quint32 MdbFile::u32(qint64 off) const
{
    const char *p = ptr(off, 4);
    return p ? qFromLittleEndian<quint32>(p) : 0;
}

qint32 MdbFile::i32(qint64 off) const
{
    return (qint32)u32(off);
}

double MdbFile::dbl(qint64 off) const
{
    const char *p = ptr(off, 8);
    if (!p)
        return 0.0;
    quint64 v = qFromLittleEndian<quint64>(p);
    double d;
    memcpy(&d, &v, 8);
    return d;
}

/* --- Jet3/Jet4 format constants --- */
static FormatConstants jet4Constants()
{
    FormatConstants c;
    c.jetVersion         = Jet4;
    c.pgSize             = 4096;
    c.rowCountOffset     = 0x0c;
    c.tabNumRowsOffset   = 16;
    c.tabNumColsOffset   = 45;
    c.tabNumIdxsOffset   = 47;
    c.tabNumRidxsOffset  = 51;
    c.tabUsageMapOffset  = 55;
    c.tabFirstDpgOffset  = 56;
    c.tabColsStartOffset = 63;
    c.tabRidxEntrySize   = 12;
    c.colScaleOffset     = 11;
    c.colPrecOffset      = 12;
    c.colFlagsOffset     = 15;
    c.colSizeOffset      = 23;
    c.colNumOffset       = 5;
    c.tabColEntrySize    = 25;
    c.tabFreeMapOffset   = 59;
    c.tabColOffsetVar    = 7;
    c.tabColOffsetFixed  = 21;
    c.tabRowColNumOffset = 9;
    c.colCountSize       = 2;
    c.defaultCodePage    = 1252;
    return c;
}

static FormatConstants jet3Constants()
{
    FormatConstants c;
    c.jetVersion         = Jet3;
    c.pgSize             = 2048;
    c.rowCountOffset     = 0x08;
    c.tabNumRowsOffset   = 12;
    c.tabNumColsOffset   = 25;
    c.tabNumIdxsOffset   = 27;
    c.tabNumRidxsOffset  = 31;
    c.tabUsageMapOffset  = 35;
    c.tabFirstDpgOffset  = 36;
    c.tabColsStartOffset = 43;
    c.tabRidxEntrySize   = 8;
    c.colScaleOffset     = 9;
    c.colPrecOffset      = 10;
    c.colFlagsOffset     = 13;
    c.colSizeOffset      = 16;
    c.colNumOffset       = 1;
    c.tabColEntrySize    = 18;
    c.tabFreeMapOffset   = 39;
    c.tabColOffsetVar    = 3;
    c.tabColOffsetFixed  = 14;
    c.tabRowColNumOffset = 5;
    c.colCountSize       = 1;
    c.defaultCodePage    = 1252;
    return c;
}

bool initFormat(MdbFile &file)
{
    /* Database definition page. Offset 0x14: Jet version: 0=Jet3,1=Jet4,... */
    const quint32 ver = file.u32(0x14);
    switch (ver & 0x0f) {
    case 0x01:
        file.fmtW() = jet4Constants();
        break;
    case 0x00:
        file.fmtW() = jet3Constants();
        break;
    default:
        return false;
    }
    /* code page for Jet3 at 0x3C */
    if (file.version() == Jet3) {
        const quint16 cp = file.u16(0x3C);
        if (cp)
            file.fmtW().defaultCodePage = cp;
    }
    file.m_numPages = file.m_data.size() / file.fmtW().pgSize;
    return file.m_numPages > 0;
}

/* --- text decoding --- */

/* Decompress Jet4 "Unicode compressed" string.
   A 0x00 byte toggles between compressed/uncompressed modes.
   In compressed mode a byte b becomes UTF-16 (b, 0x00). */
static QByteArray decompressUnicode(const QByteArray &src)
{
    QByteArray out;
    out.reserve(src.size() * 2);
    bool compress = true;
    int i = 0;
    const int n = src.size();
    while (i < n) {
        const quint8 b = (quint8)src[i];
        if (b == 0) {
            compress = !compress;
            ++i;
        } else if (compress) {
            out.append((char)b);
            out.append('\0');
            ++i;
        } else {
            if (i + 2 <= n) {
                out.append(src[i]);
                out.append(src[i + 1]);
                i += 2;
            } else {
                break;
            }
        }
    }
    return out;
}

static QString decodeText(const MdbFile &file, const QByteArray &raw)
{
    if (raw.isEmpty())
        return QString();
    if (file.version() == Jet4) {
        if (raw.size() >= 2 && (quint8)raw[0] == 0xff && (quint8)raw[1] == 0xfe) {
            QByteArray u = decompressUnicode(raw.mid(2));
            QString s = QString::fromUtf16(reinterpret_cast<const char16_t*>(u.constData()),
                                           u.size() / 2);
            return s;
        }
        QString s = QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()),
                                       raw.size() / 2);
        /* strip trailing NULs */
        int end = s.size();
        while (end > 0 && s.at(end - 1) == QChar(0))
            --end;
        return s.left(end);
    }
    /* Jet3: single byte text in code page */
    if (file.fmt().defaultCodePage == 1251) {
        /* cp1251 is not in Qt's default encodings set, use ICU-less manual table */
        static const ushort cp1251[128] = {
            0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
            0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
            0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
            0x0098,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
            0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,
            0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
            0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,
            0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
            0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,
            0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
            0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,
            0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
            0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,
            0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
            0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,
            0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
        };
        QString s;
        s.reserve(raw.size());
        for (int i = 0; i < raw.size(); ++i) {
            const quint8 b = (quint8)raw[i];
            if (b < 0x80)
                s.append(QChar(b));
            else
                s.append(QChar(cp1251[b - 0x80]));
        }
        return s;
    }
    return QString::fromLatin1(raw);
}

/* --- table definition --- */

bool readTableDef(MdbFile &file, const CatalogEntry &entry, TableDef &out)
{
    const FormatConstants &f = file.fmt();
    QByteArray pg;
    if (!file.readPage(entry.tablePg, pg))
        return false;
    if ((quint8)pg[0] != 0x02)
        return false;

    out = TableDef();
    out.page = entry.tablePg;
    out.name = entry.name;
    out.numRows  = (qint64)(quint32)qFromLittleEndian<quint32>((const uchar*)pg.constData() + f.tabNumRowsOffset);
    out.numVarCols = qFromLittleEndian<quint16>((const uchar*)pg.constData() + f.tabNumColsOffset - 2);
    out.numCols   = qFromLittleEndian<quint16>((const uchar*)pg.constData() + f.tabNumColsOffset);
    const quint32 numRidxs = qFromLittleEndian<quint32>((const uchar*)pg.constData() + f.tabNumRidxsOffset);

    /* read from a virtual continuous buffer spanning multiple tdef pages */
    QByteArray defBuf = pg;
    int curPg = entry.tablePg;
    while (qFromLittleEndian<quint32>((const uchar*)pg.constData() + 4) != 0) {
        curPg = (int)qFromLittleEndian<quint32>((const uchar*)pg.constData() + 4);
        if (!file.readPage(curPg, pg))
            break;
        defBuf += pg.mid(8);
    }

    int curPos = f.tabColsStartOffset + numRidxs * f.tabRidxEntrySize;

    out.columns.reserve(out.numCols);
    for (int i = 0; i < out.numCols; ++i) {
        if (curPos + f.tabColEntrySize > defBuf.size())
            return false;
        Column col;
        col.colType = (quint8)defBuf[curPos];
        col.colNum  = (quint8)defBuf[curPos + f.colNumOffset];
        col.varColNum = qFromLittleEndian<quint16>((const uchar*)defBuf.constData() + curPos + f.tabColOffsetVar);
        col.rowColNum = qFromLittleEndian<quint16>((const uchar*)defBuf.constData() + curPos + f.tabRowColNumOffset);
        col.flags     = (quint8)defBuf[curPos + f.colFlagsOffset];
        col.fixedOffset = qFromLittleEndian<quint16>((const uchar*)defBuf.constData() + curPos + f.tabColOffsetFixed);
        if (col.colType != COL_BOOL)
            col.colSize = qFromLittleEndian<quint16>((const uchar*)defBuf.constData() + curPos + f.colSizeOffset);
        out.columns.append(col);
        curPos += f.tabColEntrySize;
    }

    /* column names */
    for (int i = 0; i < out.numCols; ++i) {
        int nameLen;
        if (file.version() == Jet3)
            nameLen = (quint8)defBuf[curPos++];
        else
            nameLen = qFromLittleEndian<quint16>((const uchar*)defBuf.constData() + curPos), curPos += 2;
        if (curPos + nameLen > defBuf.size())
            return false;
        QByteArray raw = defBuf.mid(curPos, nameLen);
        curPos += nameLen;
        if (file.version() == Jet4) {
            QString s = QString::fromUtf16(reinterpret_cast<const char16_t*>(raw.constData()),
                                           raw.size() / 2);
            int end = s.size();
            while (end > 0 && s.at(end - 1) == QChar(0))
                --end;
            out.columns[i].name = s.left(end);
        } else {
            out.columns[i].name = decodeText(file, raw);
        }
    }

    /* NOTE: keep the natural tdef order (matches the source column order).
       The null-mask logic uses col.colNum, field positions use the absolute
       fixedOffset/varColNum, so array order does not affect decoding. */
    return true;
}

/* --- data pages --- */

/* Find all data pages belonging to a table by scanning the file.
   A data page has page_type==0x01 and a 4-byte tdef pointer equal to
   the table definition page. */
QVector<int> tableDataPages(const MdbFile &file, const TableDef &table)
{
    QVector<int> pages;
    const int n = file.numPages();
    const int ps = file.pageSize();
    for (int pg = 0; pg < n; ++pg) {
        const qint64 off = (qint64)pg * ps;
        const char *p = file.ptr(off, 8);
        if (!p)
            break;
        if (p[0] != 0x01)
            continue;
        if ((qint32)qFromLittleEndian<quint32>((const uchar*)p + 4) != table.page)
            continue;
        pages.append(pg);
    }
    return pages;
}

/* --- row decoding --- */

struct RowField {
    int     colnum = 0;
    bool    isFixed = false;
    bool    isNull = true;
    int     start = 0;
    int     size = 0;
    bool    boolValue = false;
};

/* Decode a row into raw field locations. Returns number of fields or -1. */
static int crackRow(const MdbFile &file, int page, int row,
                    const TableDef &table, QVector<RowField> &fields)
{    const FormatConstants &f = file.fmt();
    QByteArray pg;
    if (!file.readPage(page, pg))
        return -1;

    const int rco = f.rowCountOffset;
    const int pgSize = f.pgSize;
    if (row < 0 || row > 1000)
        return -1;

    int rowStart = (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rco + 2 + row*2);
    int nextStart = (row == 0) ? pgSize :
        (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rco + row*2);

    /* 0x4000: deleted row (skip); 0x8000: index lookup row (keep) */
    if (rowStart & 0x4000)
        return -1;

    rowStart &= 0x3fff;
    nextStart &= 0x3fff;
    if (rowStart >= pgSize || rowStart > nextStart || nextStart > pgSize)
        return -1;
    const int rowEnd = nextStart - 1;
    const int rowSize = nextStart - rowStart;

    int rowCols;
    if (file.version() == Jet3)
        rowCols = (quint8)pg[rowStart];
    else
        rowCols = (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rowStart);

    const int bitmaskSz = (rowCols + 7) / 8;
    if (bitmaskSz + f.colCountSize >= rowEnd)
        return -1;

    const uchar *nullmask = (const uchar*)pg.constData() + rowEnd - bitmaskSz + 1;

    int rowVarCols = 0;
    QVector<int> varOffsets;
    if (table.numVarCols > 0) {
        if (file.version() == Jet3) {
            rowVarCols = (quint8)pg[rowEnd - bitmaskSz];
            varOffsets.resize(rowVarCols + 1);
            /* Jet3 jump-table based offsets */
            const int numJumps = (rowSize - 1) / 256;
            int jumpsUsed = 0;
            for (int i = 0; i <= rowVarCols; ++i) {
                while (jumpsUsed < numJumps &&
                       i == (quint8)pg[rowEnd - bitmaskSz - jumpsUsed - 1])
                    ++jumpsUsed;
                varOffsets[i] = (quint8)pg[rowEnd - bitmaskSz - numJumps - 1 - i] + jumpsUsed * 256;
            }
        } else {
            /* guard: the var offset table is read backwards from rowEnd and
               must stay within the row */
            const int availBytes = rowEnd - bitmaskSz - 1 - rowStart;
            const int maxVarCols = qMax(0, (availBytes - 1) / 2);
            rowVarCols = (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rowEnd - bitmaskSz - 1);
            if (rowVarCols > maxVarCols)
                rowVarCols = maxVarCols;
            varOffsets.resize(rowVarCols + 1);
            for (int i = 0; i <= rowVarCols; ++i)
                varOffsets[i] = (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rowEnd - bitmaskSz - 3 - i*2);
        }
    }

    int fixedFound = 0;
    const int rowFixedCols = rowCols - rowVarCols;

    fields.resize(table.columns.size());
    for (int i = 0; i < table.columns.size(); ++i) {
        const Column &col = table.columns[i];
        RowField &rf = fields[i];
        rf.colnum = i;
        rf.isFixed = col.isFixed();
        const int byteNum = col.colNum / 8;
        const int bitNum  = col.colNum % 8;
        const bool bitSet = (nullmask[byteNum] & (1 << bitNum)) != 0;

        if (col.colType == COL_BOOL) {
            rf.isNull = false;
            rf.boolValue = bitSet;
            rf.start = 0;
            rf.size = 0;
            continue;
        }
        rf.isNull = !bitSet;
        if (col.isFixed() && fixedFound < rowFixedCols) {
            rf.start = rowStart + col.fixedOffset + f.colCountSize;
            rf.size = col.colSize;
            ++fixedFound;
        } else if (!col.isFixed() && col.varColNum < rowVarCols) {
            rf.start = rowStart + varOffsets[col.varColNum];
            rf.size = varOffsets[col.varColNum + 1] - varOffsets[col.varColNum];
        } else {
            rf.isNull = true;
            rf.start = 0;
            rf.size = 0;
        }
        if (rf.start + rf.size > rowStart + rowSize) {
            rf.isNull = true;
            rf.start = 0;
            rf.size = 0;
        }
    }
    return rowCols;
}

/* Read the value bytes for an OLE/memo field (following LVAL pages). */
static QByteArray readOleValue(const MdbFile &file, int page, int start, int size,
                               QByteArray *rawHeaderOut = nullptr)
{
    QByteArray header = file.bytes((qint64)page * file.pageSize() + start,
                                   qMin(size, 12));
    if (rawHeaderOut)
        *rawHeaderOut = header;

    /* Header: 3 bytes length, 1 byte bitmask, 4 bytes lval_dp, 4 bytes unknown */
    if (header.size() < 8)
        return QByteArray();
    const quint32 len = qFromLittleEndian<quint32>((const uchar*)header.constData());
    const quint8 mask = (quint8)header[3];
    QByteArray result;

    if (len & 0x80000000) {
        /* inline value follows the 12-byte header */
        return file.bytes((qint64)page * file.pageSize() + start + 12,
                          qMax(0, size - 12));
    }

    const quint32 pgRow = qFromLittleEndian<quint32>((const uchar*)header.constData() + 4);
    if (!pgRow)
        return QByteArray();
    int lvalPg = (int)(pgRow >> 8);
    int lvalRow = (int)(pgRow & 0xff);

    if (len & 0x40000000) {
        /* single LVAL page */
        return readRowBytes(file, lvalPg, lvalRow);
    }

    /* chained LVAL pages (type 2) */
    while (lvalPg) {
        QByteArray chunk = readRowBytes(file, lvalPg, lvalRow);
        if (chunk.isEmpty())
            break;
        /* first 4 bytes: next page/row pointer */
        if (chunk.size() >= 4) {
            const quint32 next = qFromLittleEndian<quint32>((const uchar*)chunk.constData());
            result += chunk.mid(4);
            lvalPg = (int)(next >> 8);
            lvalRow = (int)(next & 0xff);
        } else {
            result += chunk;
            break;
        }
        if (result.size() > (int)(len & 0x00ffffff) + 16)
            break;
    }
    return result;
}

QByteArray readRowBytes(const MdbFile &file, int page, int row)
{
    const FormatConstants &f = file.fmt();
    QByteArray pg;
    if (!file.readPage(page, pg))
        return QByteArray();
    const int rco = f.rowCountOffset;
    int rowStart = (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rco + 2 + row*2);
    int nextStart = (row == 0) ? f.pgSize :
        (int)qFromLittleEndian<quint16>((const uchar*)pg.constData() + rco + row*2);
    rowStart &= 0x3fff;
    nextStart &= 0x3fff;
    if (rowStart >= f.pgSize || rowStart > nextStart || nextStart > f.pgSize)
        return QByteArray();
    return pg.mid(rowStart, nextStart - rowStart);
}

/* Decode a single field value into a QVariant. */
static QVariant decodeField(const MdbFile &file, int page, const Column &col,
                            const RowField &rf)
{
    const FormatConstants &f = file.fmt();
    if (rf.isNull)
        return QVariant();

    const qint64 pageOff = (qint64)page * f.pgSize;
    const char *p = file.ptr(pageOff + rf.start, rf.size);

    switch (col.colType) {
    case COL_BOOL:
        return QVariant(rf.boolValue);
    case COL_BYTE:
        return p ? QVariant((quint8)p[0]) : QVariant();
    case COL_INT:
        return p ? QVariant((int)qFromLittleEndian<qint16>(p)) : QVariant();
    case COL_LONGINT:
        return p ? QVariant((qlonglong)qFromLittleEndian<qint32>(p)) : QVariant();
    case COL_MONEY: {
        if (!p) return QVariant();
        qint64 v = (qint64)qFromLittleEndian<qint64>(p);
        return QVariant((double)v / 10000.0);
    }
    case COL_FLOAT: {
        if (!p) return QVariant();
        quint32 b = qFromLittleEndian<quint32>(p);
        float fl; memcpy(&fl, &b, 4);
        return QVariant((double)fl);
    }
    case COL_DOUBLE:
        return p ? QVariant(file.dbl(pageOff + rf.start)) : QVariant();
    case COL_DATETIME: {
        double d = file.dbl(pageOff + rf.start);
        /* OLE Automation date: days since 1899-12-30 */
        const qint64 ms = (qint64)(d * 86400000.0);
        return QVariant(QDateTime::fromMSecsSinceEpoch(ms - 2208988800000LL, Qt::UTC));
    }
    case COL_TEXT: {
        QByteArray raw = file.bytes(pageOff + rf.start, rf.size);
        return QVariant(decodeText(file, raw));
    }
    case COL_OLE:
    case COL_MEMO: {
        QByteArray raw = file.bytes(pageOff + rf.start, rf.size);
        if (rf.size > 12) {
            /* try inline first */
            const quint32 len = qFromLittleEndian<quint32>((const uchar*)raw.constData());
            if (len & 0x80000000)
                return QVariant(decodeText(file, raw.mid(12)));
        }
        QByteArray val = readOleValue(file, page, rf.start, rf.size);
        if (col.colType == COL_MEMO)
            return QVariant(decodeText(file, val));
        return QVariant(val);
    }
    case COL_BINARY:
        return p ? QVariant(file.bytes(pageOff + rf.start, rf.size)) : QVariant();
    case COL_REPID:
        return p ? QVariant(file.bytes(pageOff + rf.start, qMin(rf.size, 16))) : QVariant();
    case COL_NUMERIC: {
        if (!p || rf.size < 17)
            return QVariant();
        /* Jet NUMERIC: scale byte at [0], 16 bytes big-endian? */
        const qint8 scale = (qint8)p[0];
        QByteArray be(p + 1, 16);
        qlonglong mant = 0;
        for (int i = 0; i < 16; ++i)
            mant = (mant << 8) | (quint8)be[i];
        double d = (double)mant;
        for (int i = 0; i < scale; ++i)
            d /= 10.0;
        return QVariant(d);
    }
    default:
        return p ? QVariant(file.bytes(pageOff + rf.start, rf.size)) : QVariant();
    }
}

bool readRowValues(const MdbFile &file, int page, int row, const TableDef &table,
                   QVector<QVariant> &out)
{
    QVector<RowField> fields;
    if (crackRow(file, page, row, table, fields) < 0)
        return false;
    out.resize(table.columns.size());
    for (int i = 0; i < table.columns.size(); ++i)
        out[i] = decodeField(file, page, table.columns[i], fields[i]);
    return true;
}

/* --- catalog --- */

bool readCatalog(MdbFile &file, QVector<CatalogEntry> &out)
{
    CatalogEntry msys;
    msys.name = "MSysObjects";
    msys.type = 1;
    msys.flags = 0;
    msys.tablePg = 2;

    TableDef tdef;
    if (!readTableDef(file, msys, tdef))
        return false;

    /* find Id, Name, Type, Flags columns */
    int colId = -1, colName = -1, colType = -1, colFlags = -1;
    for (int i = 0; i < tdef.columns.size(); ++i) {
        const QString &n = tdef.columns[i].name;
        if (n == "Id") colId = i;
        else if (n == "Name") colName = i;
        else if (n == "Type") colType = i;
        else if (n == "Flags") colFlags = i;
    }
    if (colId < 0 || colName < 0 || colType < 0)
        return false;

    const QVector<int> pages = tableDataPages(file, tdef);
    out.clear();
    for (int pg : pages) {
        QByteArray pgb;
        if (!file.readPage(pg, pgb))
            continue;
        const int nrows = (int)qFromLittleEndian<quint16>((const uchar*)pgb.constData() + file.fmt().rowCountOffset);
        for (int r = 0; r < nrows; ++r) {
            QVector<QVariant> vals;
            if (!readRowValues(file, pg, r, tdef, vals))
                continue;
            if (vals.size() <= colId || vals.size() <= colName || vals.size() <= colType)
                continue;
            const QString name = vals[colName].toString();
            if (name.isEmpty())
                continue;
            CatalogEntry e;
            e.name = name;
            e.type = vals[colType].toInt() & 0x7f;
            e.flags = (quint32)vals[colFlags].toInt();
            e.tablePg = (int)(vals[colId].toLongLong() & 0x00ffffff);
            out.append(e);
        }
    }
    return true;
}

QString jetVersionString(JetVersion v)
{
    switch (v) {
    case Jet3: return "Jet3";
    case Jet4: return "Jet4";
    default:   return "unknown";
    }
}

QString valueToString(const QVariant &v)
{
    if (!v.isValid())
        return QStringLiteral("NULL");
    switch (v.type()) {
    case QMetaType::QByteArray:
        return QStringLiteral("<%1 bytes>").arg(v.toByteArray().size());
    case QMetaType::QDateTime:
        return v.toDateTime().toString(Qt::ISODate);
    case QMetaType::Bool:
        return v.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    default:
        return v.toString();
    }
}

} // namespace mdb
