#pragma once

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QTableWidget;
class QPushButton;

class ArchiveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ArchiveDialog(const QString &baseDir, int bookId, QWidget *parent = nullptr);

private slots:
    void findDuplicates();
    void archiveSelected();

private:
    int selectedId() const;

    QString m_baseDir;
    int m_bookId = 0;

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_authorEdit = nullptr;
    QLabel *m_summary = nullptr;
    QTableWidget *m_table = nullptr;
    QCheckBox *m_deleteFile = nullptr;
    QLineEdit *m_noteEdit = nullptr;
    QPushButton *m_archiveButton = nullptr;
};
