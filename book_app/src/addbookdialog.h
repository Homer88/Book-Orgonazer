#pragma once

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QCheckBox;
class QLineEdit;
class QComboBox;
class QSpinBox;
class QPlainTextEdit;
class QSqlQuery;

class AddBookDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddBookDialog(QWidget *parent = nullptr);

    void setBaseDir(const QString &dir);
    void setNoImagePath(const QString &path);
    void setFileFilters(const QStringList &filters);
    void setReferenceData(const QStringList &formats, const QStringList &languages,
                          const QStringList &temas, const QStringList &systems,
                          const QStringList &studios, const QStringList &types,
                          const QStringList &sources);

    QString commit();

private slots:
    void browseBook();
    void browseCover();

private:
    QWidget *buildForm();
    void processSelectedFile();
    QString md5OfFile(const QString &path) const;
    int langCode() const;
    QString langString() const;

    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_authorEdit = nullptr;
    QSpinBox *m_yearSpin = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QComboBox *m_langCombo = nullptr;
    QComboBox *m_temaCombo = nullptr;
    QComboBox *m_systemCombo = nullptr;
    QComboBox *m_studioCombo = nullptr;
    QComboBox *m_vidCombo = nullptr;
    QComboBox *m_sourceCombo = nullptr;
    QSpinBox *m_pagesSpin = nullptr;
    QLineEdit *m_izdatelEdit = nullptr;
    QLineEdit *m_sizeEdit = nullptr;
    QLineEdit *m_crcEdit = nullptr;
    QLineEdit *m_partEdit = nullptr;
    QLineEdit *m_imageEdit = nullptr;

    QList<QCheckBox *> m_langChecks;

    QLineEdit *m_fileEdit = nullptr;
    QLineEdit *m_coverFileEdit = nullptr;
    QPlainTextEdit *m_descEdit = nullptr;

    QString m_selectedFile;
    QString m_selectedCover;
    QString m_md5;
    QString m_baseDir;
    QString m_noImagePath;
    QStringList m_fileFilters;
};
