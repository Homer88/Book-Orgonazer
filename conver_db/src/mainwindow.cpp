#include "mainwindow.h"
#include "converter.h"

#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Конвертер баз данных Access -> SQLite"));
    resize(640, 480);
    setCentralWidget(buildUi());

    m_thread = new QThread(this);
    m_converter = new Converter;
    m_converter->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_converter, &QObject::deleteLater);
    connect(this, &MainWindow::convertRequested, m_converter, &Converter::convert);
    connect(m_converter, &Converter::logMessage, this, &MainWindow::onLog);
    connect(m_converter, &Converter::errorMessage, this, &MainWindow::onError);
    connect(m_converter, &Converter::progressChanged, this, &MainWindow::onProgress);
    connect(m_converter, &Converter::finished, this, &MainWindow::onFinished);
    m_thread->start();
}

MainWindow::~MainWindow()
{
    /* Converter is deleted by the thread's finished->deleteLater connection,
       so only stop the worker thread here. */
    m_thread->quit();
    m_thread->wait();
}

QWidget *MainWindow::buildUi()
{
    auto *central = new QWidget(this);

    m_inputEdit = new QLineEdit;
    m_inputEdit->setPlaceholderText(QStringLiteral("Путь к файлу базы данных Access (.mdb)"));
    auto *inBrowse = new QPushButton(QStringLiteral("Обзор..."));
    connect(inBrowse, &QPushButton::clicked, this, &MainWindow::browseInput);

    auto *inRow = new QHBoxLayout;
    inRow->addWidget(m_inputEdit, 1);
    inRow->addWidget(inBrowse);

    auto *inBox = new QGroupBox(QStringLiteral("Входная база данных Access"));
    auto *inBoxL = new QVBoxLayout(inBox);
    inBoxL->addLayout(inRow);

    m_outputEdit = new QLineEdit;
    m_outputEdit->setPlaceholderText(QStringLiteral("Путь к выходному файлу SQLite (.sqlite)"));
    auto *outBrowse = new QPushButton(QStringLiteral("Обзор..."));
    connect(outBrowse, &QPushButton::clicked, this, &MainWindow::browseOutput);

    auto *outRow = new QHBoxLayout;
    outRow->addWidget(m_outputEdit, 1);
    outRow->addWidget(outBrowse);

    auto *outBox = new QGroupBox(QStringLiteral("Выходная база данных SQLite"));
    auto *outBoxL = new QVBoxLayout(outBox);
    outBoxL->addLayout(outRow);

    m_convertBtn = new QPushButton(QStringLiteral("Конвертировать"));
    connect(m_convertBtn, &QPushButton::clicked, this, &MainWindow::onConvert);

    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);

    m_statusLabel = new QLabel(QStringLiteral("Готов"));

    auto *layout = new QVBoxLayout(central);
    layout->addWidget(inBox);
    layout->addWidget(outBox);
    layout->addWidget(m_convertBtn);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_log, 1);

    return central;
}

void MainWindow::startConversion(const QString &inPath, const QString &outPath)
{
    if (!inPath.isEmpty())
        m_inputEdit->setText(inPath);
    if (!outPath.isEmpty())
        m_outputEdit->setText(outPath);
}

void MainWindow::browseInput()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Выберите базу данных Access"), QString(),
        QStringLiteral("Базы Access (*.mdb *.accdb);;Все файлы (*)"));
    if (path.isEmpty())
        return;
    m_inputEdit->setText(path);
    if (m_outputEdit->text().isEmpty()) {
        const QFileInfo fi(path);
        m_outputEdit->setText(fi.absolutePath() + QLatin1Char('/') +
                              fi.completeBaseName() + QStringLiteral(".sqlite"));
    }
}

void MainWindow::browseOutput()
{
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Выберите файл базы данных SQLite"), QString(),
        QStringLiteral("Базы SQLite (*.sqlite *.db);;Все файлы (*)"));
    if (path.isEmpty())
        return;
    m_outputEdit->setText(path);
}

void MainWindow::onConvert()
{
    if (m_busy)
        return;
    const QString in = m_inputEdit->text().trimmed();
    const QString out = m_outputEdit->text().trimmed();
    if (in.isEmpty() || out.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Укажите входной и выходной файл."));
        return;
    }

    m_busy = true;
    m_convertBtn->setEnabled(false);
    m_inputEdit->setEnabled(false);
    m_outputEdit->setEnabled(false);
    m_log->clear();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_statusLabel->setText(QStringLiteral("Конвертация..."));
    emit convertRequested(in, out);
}

void MainWindow::onLog(const QString &msg)
{
    m_log->appendPlainText(msg);
}

void MainWindow::onError(const QString &msg)
{
    m_log->appendPlainText(QStringLiteral("[Ошибка] ") + msg);
}

void MainWindow::onProgress(int done, int total)
{
    if (total <= 0) {
        m_progressBar->setRange(0, 0);
        return;
    }
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(done);
}

void MainWindow::onFinished(bool ok, const QString &message)
{
    m_busy = false;
    m_convertBtn->setEnabled(true);
    m_inputEdit->setEnabled(true);
    m_outputEdit->setEnabled(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(ok ? 100 : 0);
    m_statusLabel->setText(message);

    if (ok)
        QMessageBox::information(this, QStringLiteral("Готово"), message);
    else
        QMessageBox::critical(this, QStringLiteral("Ошибка"), message);
}
