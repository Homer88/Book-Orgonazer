#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLineEdit;
class QPushButton;
class QProgressBar;
class QPlainTextEdit;
class QLabel;
class QThread;
class Converter;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void startConversion(const QString &inPath, const QString &outPath);

signals:
    void convertRequested(const QString &inPath, const QString &outPath);

private slots:
    void browseInput();
    void browseOutput();
    void onConvert();
    void onLog(const QString &msg);
    void onError(const QString &msg);
    void onProgress(int done, int total);
    void onFinished(bool ok, const QString &message);

private:
    QWidget *buildUi();

    QLineEdit *m_inputEdit = nullptr;
    QLineEdit *m_outputEdit = nullptr;
    QPushButton *m_convertBtn = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_statusLabel = nullptr;
    QThread *m_thread = nullptr;
    Converter *m_converter = nullptr;
    bool m_busy = false;
};

#endif // MAINWINDOW_H
