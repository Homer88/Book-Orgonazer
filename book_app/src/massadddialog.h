#pragma once

#include <QDialog>
#include <QList>
#include <QString>

class QTableWidget;
class QLineEdit;
class QProgressBar;
class QLabel;
class QPushButton;

struct ScannedBook {
    QString filePath;
    QString fileName;
    QString nameBook;
    QString format;
    QString sizeText;
    QString crc;
    QString coverPath;
    QString zipPath;
    bool duplicate = false;
};

class MassAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MassAddDialog(const QString &baseDir, QWidget *parent = nullptr);

private slots:
    void browseFolder();
    void scanFolder();
    void addSelected();
    void toggleAll();

private:
    void scanRecursive(const QString &dir, QList<ScannedBook> *books);
    QString computeCrc(const QString &path) const;
    void populateTable(const QList<ScannedBook> &books);
    QString nextIndex() const;

    QString m_baseDir;
    QLineEdit *m_folderEdit = nullptr;
    QTableWidget *m_table = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_toggleButton = nullptr;
    int m_newCount = 0;
    int m_dupCount = 0;
};
