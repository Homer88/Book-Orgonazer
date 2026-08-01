#pragma once

#include "configloader.h"

#include <QMainWindow>
#include <QModelIndex>
#include <QString>

class QSqlQueryModel;
class QTableView;
class QLineEdit;
class QComboBox;
class QLabel;
class QTextBrowser;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void refreshQuery();
    void onSelectionChanged(const QModelIndex &current);
    void showBookDetails(int row);
    void addBook();
    void editBook();
    void openBook();
    void openBookFolder();
    void resetFilters();
    void pickRandomBook();
    void clearStatus();

private:
    QWidget *buildFilterBar();
    QWidget *buildDetailsPanel();
    void loadConfig();
    QString buildWhereClause(QStringList *values) const;
    QString bookDir(const QString &part) const;
    QString bookFile(const QString &part, const QString &ahtor,
                     const QString &format) const;
    QString resolveBaseDir() const;

    ConfigLoader m_config;
    QString m_configPath;
    QString m_dbPath;

    QSqlQueryModel *m_model = nullptr;
    QTableView *m_table = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_temaCombo = nullptr;
    QComboBox *m_systemCombo = nullptr;
    QComboBox *m_studioCombo = nullptr;
    QComboBox *m_vidCombo = nullptr;
    QComboBox *m_sourceCombo = nullptr;
    QComboBox *m_statusCombo = nullptr;

    QLabel *m_coverLabel = nullptr;
    QTextBrowser *m_detailsBrowser = nullptr;
    QLabel *m_statusLabel = nullptr;

    QString m_baseDir;
    QString m_noImagePath;
};
