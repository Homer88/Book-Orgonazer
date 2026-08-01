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

class EditBookDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditBookDialog(QWidget *parent = nullptr);

    void setBookIndex(int index);
    void setBaseDir(const QString &dir);
    void setFileFilters(const QStringList &filters);
    void setReferenceData(const QStringList &formats, const QStringList &languages,
                          const QStringList &temas, const QStringList &systems,
                          const QStringList &studios, const QStringList &types,
                          const QStringList &sources);

    QString commit();
    void loadBook();

private slots:
    void browseNewFile();

private:
    QWidget *buildForm();
    int langCode() const;
    QString langString() const;
    void setLangChecks(int code);
    QString md5OfFile(const QString &path) const;

    int m_index = 0;
    QString m_baseDir;

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
    QLineEdit *m_partEdit = nullptr;
    QLineEdit *m_imageEdit = nullptr;
    QPlainTextEdit *m_descEdit = nullptr;

    QList<QCheckBox *> m_langChecks;

    QLineEdit *m_fileEdit = nullptr;
    QLineEdit *m_sizeEdit = nullptr;
    QLineEdit *m_crcEdit = nullptr;

    QStringList m_fileFilters;
    QString m_selectedFile;
    QString m_md5;

    QString m_oldAuthor;
    QString m_oldFormat;
};
