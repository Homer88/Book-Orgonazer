#include "mainwindow.h"
#include "addbookdialog.h"
#include "editbookdialog.h"
#include "massadddialog.h"
#include "settings.h"
#include "settingsdialog.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QModelIndex>
#include <QItemSelectionModel>
#include <QPushButton>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QSplitter>
#include <QStatusBar>
#include <QTableView>
#include <QTextBrowser>
#include <QTextCodec>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char *const kEmpty = "(все)";

QString findProjectRoot()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        const QString root = dir.absolutePath();
        if (QFileInfo::exists(root + QStringLiteral("/config.ini")) &&
            QFileInfo::exists(root + QStringLiteral("/db/book.sqlite")))
            return root;
        if (!dir.cdUp())
            break;
    }
    return QFileInfo(QCoreApplication::applicationFilePath()).dir()
        .absoluteFilePath(QStringLiteral(".."));
}

QSqlQueryModel *makeModel(QObject *parent)
{
    return new QSqlQueryModel(parent);
}

void applyHeaders(QSqlQueryModel *model)
{
    model->setHeaderData(0, Qt::Horizontal, QObject::tr("№"));
    model->setHeaderData(1, Qt::Horizontal, QObject::tr("Название"));
    model->setHeaderData(2, Qt::Horizontal, QObject::tr("Автор"));
    model->setHeaderData(3, Qt::Horizontal, QObject::tr("Год"));
    model->setHeaderData(4, Qt::Horizontal, QObject::tr("Формат"));
    model->setHeaderData(5, Qt::Horizontal, QObject::tr("Язык"));
    model->setHeaderData(6, Qt::Horizontal, QObject::tr("Стр."));
    model->setHeaderData(7, Qt::Horizontal, QObject::tr("Размер"));
    model->setHeaderData(8, Qt::Horizontal, QObject::tr("Наличие"));
    model->setHeaderData(9, Qt::Horizontal, QObject::tr("Издатель"));
    model->setHeaderData(12, Qt::Horizontal, QObject::tr("Статус"));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString projectRoot = findProjectRoot();
    m_configPath = projectRoot + QStringLiteral("/config.ini");
    if (!QFileInfo::exists(m_configPath))
        m_configPath = appDir + QStringLiteral("/config.ini");
    loadConfig();

    m_dbPath = projectRoot + QStringLiteral("/db/book.sqlite");
    if (!QFileInfo::exists(m_dbPath))
        m_dbPath = appDir + QStringLiteral("/db/book.sqlite");

    setWindowTitle(QStringLiteral("Органайзер книг"));
    resize(1200, 700);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(buildFilterBar());

    m_table = new QTableView;
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setSortingEnabled(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    splitter->addWidget(m_table);

    splitter->addWidget(buildDetailsPanel());
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 1);
    setCentralWidget(splitter);

    QStatusBar *sb = statusBar();
    m_statusLabel = new QLabel;
    sb->addWidget(m_statusLabel);

    auto *toolBar = addToolBar(QStringLiteral("Главная"));
    toolBar->setMovable(false);
    auto *addAction = toolBar->addAction(QStringLiteral("Добавить книгу"));
    addAction->setToolTip(QStringLiteral("Добавить новую книгу в базу"));
    connect(addAction, &QAction::triggered, this, &MainWindow::addBook);
    auto *openAction = toolBar->addAction(QStringLiteral("Открыть книгу"));
    openAction->setToolTip(QStringLiteral("Открыть файл книги"));
    connect(openAction, &QAction::triggered, this, &MainWindow::openBook);
    auto *openDirAction = toolBar->addAction(QStringLiteral("Открыть папку"));
    openDirAction->setToolTip(QStringLiteral("Открыть папку с книгой"));
    connect(openDirAction, &QAction::triggered, this, &MainWindow::openBookFolder);
    auto *editAction = toolBar->addAction(QStringLiteral("Редактировать"));
    editAction->setToolTip(QStringLiteral("Изменить информацию о книге"));
    connect(editAction, &QAction::triggered, this, &MainWindow::editBook);
    auto *resetAction = toolBar->addAction(QStringLiteral("Сбросить фильтры"));
    connect(resetAction, &QAction::triggered, this, &MainWindow::resetFilters);
    auto *randomAction = toolBar->addAction(QStringLiteral("Что почитать?"));
    randomAction->setToolTip(QStringLiteral("Случайная книга из текущего фильтра"));
    connect(randomAction, &QAction::triggered, this, &MainWindow::pickRandomBook);
    auto *clearStatusAction = toolBar->addAction(QStringLiteral("Сбросить статус"));
    clearStatusAction->setToolTip(QStringLiteral("Снять отметку «Читаю»/«Перевожу» с выбранной книги"));
    connect(clearStatusAction, &QAction::triggered, this, &MainWindow::clearStatus);
    auto *settingsAction = toolBar->addAction(QStringLiteral("Настройки"));
    settingsAction->setToolTip(QStringLiteral("Настройки приложения (шрифт, масштаб)"));
    connect(settingsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            refreshQuery();
            setMinimumSize(QSize(qRound(800 * SettingsManager::instance().scale()),
                                 qRound(500 * SettingsManager::instance().scale())));
            resize(qRound(1200 * SettingsManager::instance().scale()),
                   qRound(700 * SettingsManager::instance().scale()));
        }
    });

    auto *massAddAction = toolBar->addAction(QStringLiteral("Массовое добавление"));
    massAddAction->setToolTip(QStringLiteral("Сканировать папку и добавить новые книги"));
    connect(massAddAction, &QAction::triggered, this, [this]() {
        MassAddDialog dlg(m_baseDir, this);
        dlg.exec();
        refreshQuery();
    });

    {
        auto &s = SettingsManager::instance();
        s.load();
        QApplication::setFont(s.appFont());
        setMinimumSize(QSize(qRound(800 * s.scale()), qRound(500 * s.scale())));
        resize(qRound(1200 * s.scale()), qRound(700 * s.scale()));
    }

    refreshQuery();
}

void MainWindow::loadConfig()
{
    m_config.load(m_configPath);
    if (m_config.isValid()) {
        m_baseDir = m_config.basePath();
        m_noImagePath = QFileInfo(m_configPath).dir().absoluteFilePath(QStringLiteral("no.jpg"));
    } else {
        m_baseDir.clear();
        m_noImagePath.clear();
    }
}

QString MainWindow::resolveBaseDir() const
{
    if (!m_baseDir.isEmpty() && QDir(m_baseDir).exists())
        return m_baseDir;
    return QFileInfo(m_configPath).dir().absolutePath();
}

QWidget *MainWindow::buildFilterBar()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(4, 4, 4, 4);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Поиск: название, автор, издатель, язык..."));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshQuery);

    auto makeCombo = [this](const QStringList &items) {
        auto *combo = new QComboBox;
        combo->addItem(QString::fromUtf8(kEmpty));
        combo->addItems(items);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::refreshQuery);
        return combo;
    };

    m_formatCombo = makeCombo(m_config.values(QStringLiteral("Format")));
    m_langCombo = makeCombo(m_config.values(QStringLiteral("Lang")));
    m_temaCombo = makeCombo(m_config.values(QStringLiteral("Tema")));
    m_systemCombo = makeCombo(m_config.values(QStringLiteral("System")));
    m_studioCombo = makeCombo(m_config.values(QStringLiteral("Studio")));
    m_vidCombo = makeCombo(m_config.values(QStringLiteral("Tipog")));
    m_sourceCombo = makeCombo(m_config.values(QStringLiteral("Application")));
    m_statusCombo = makeCombo({QStringLiteral("Читаю"),
                               QStringLiteral("Перевожу"),
                               QStringLiteral("Без статуса")});

    auto addFilterRow = [&layout](const QString &label, QWidget *w) {
        auto *row = new QHBoxLayout;
        auto *l = new QLabel(label);
        row->addWidget(l);
        row->addWidget(w, 1);
        layout->addLayout(row);
    };

    addFilterRow(QStringLiteral("Формат:"), m_formatCombo);
    addFilterRow(QStringLiteral("Язык:"), m_langCombo);
    addFilterRow(QStringLiteral("Тематика:"), m_temaCombo);
    addFilterRow(QStringLiteral("Система:"), m_systemCombo);
    addFilterRow(QStringLiteral("Студия:"), m_studioCombo);
    addFilterRow(QStringLiteral("Тип:"), m_vidCombo);
    addFilterRow(QStringLiteral("Наличие:"), m_sourceCombo);
    addFilterRow(QStringLiteral("Статус:"), m_statusCombo);

    layout->addWidget(m_searchEdit, 1);
    return panel;
}

QWidget *MainWindow::buildDetailsPanel()
{
    auto *panel = new QWidget(this);
    auto *layout = new QVBoxLayout(panel);

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *coverHost = new QWidget;
    auto *coverLayout = new QVBoxLayout(coverHost);
    m_coverLabel = new QLabel;
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setMinimumHeight(300);
    m_coverLabel->setText(QStringLiteral("Нет обложки"));
    coverLayout->addWidget(m_coverLabel);

    auto *coverBtns = new QHBoxLayout;
    auto *btnPrev = new QPushButton(QStringLiteral("<<"));
    auto *btnMinus = new QPushButton(QStringLiteral("−"));
    auto *btnPlus = new QPushButton(QStringLiteral("+"));
    auto *btnNext = new QPushButton(QStringLiteral(">>"));
    btnPrev->setToolTip(QStringLiteral("Предыдущая обложка"));
    btnMinus->setToolTip(QStringLiteral("Уменьшить"));
    btnPlus->setToolTip(QStringLiteral("Увеличить"));
    btnNext->setToolTip(QStringLiteral("Следующая обложка"));
    connect(btnPrev, &QPushButton::clicked, this, &MainWindow::prevCover);
    connect(btnMinus, &QPushButton::clicked, this, &MainWindow::zoomOut);
    connect(btnPlus, &QPushButton::clicked, this, &MainWindow::zoomIn);
    connect(btnNext, &QPushButton::clicked, this, &MainWindow::nextCover);
    coverBtns->addStretch(1);
    coverBtns->addWidget(btnPrev);
    coverBtns->addWidget(btnMinus);
    coverBtns->addWidget(btnPlus);
    coverBtns->addWidget(btnNext);
    coverBtns->addStretch(1);
    coverLayout->addLayout(coverBtns);

    coverLayout->addStretch(1);
    scroll->setWidget(coverHost);
    layout->addWidget(scroll, 2);

    m_detailsBrowser = new QTextBrowser;
    m_detailsBrowser->setOpenExternalLinks(false);
    layout->addWidget(m_detailsBrowser, 3);

    return panel;
}

QString MainWindow::bookDir(const QString &part) const
{
    QString rel = part.trimmed();
    if (rel.isEmpty())
        return QString();
    rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (rel.startsWith(QLatin1Char('/')))
        rel.remove(0, 1);
    return QDir(resolveBaseDir()).absoluteFilePath(rel);
}

QString MainWindow::bookFile(const QString &part, const QString &ahtor,
                             const QString &format) const
{
    const QString dir = bookDir(part);
    if (dir.isEmpty())
        return QString();
    return QDir(dir).filePath(ahtor + format);
}

QString MainWindow::buildWhereClause(QStringList *values) const
{
    QStringList conds;

    const QString search = m_searchEdit->text().trimmed();
    if (!search.isEmpty()) {
        conds << QStringLiteral("(name_book LIKE ? ESCAPE '\\' OR ahtor LIKE ? "
                                "OR izdatel LIKE ? OR lang_string LIKE ? "
                                "OR langav LIKE ?)");
        const QString pattern = QLatin1Char('%') + search + QLatin1Char('%');
        for (int i = 0; i < 5; ++i)
            values->append(pattern);
    }

    auto addCond = [&conds, values](const QComboBox *combo, const char *column) {
        if (!combo || combo->currentIndex() <= 0)
            return;
        conds << QStringLiteral("%1 = ?").arg(QLatin1String(column));
        values->append(combo->currentText());
    };

    addCond(m_formatCombo, "format");
    addCond(m_langCombo, "langav");
    addCond(m_temaCombo, "tematica");
    addCond(m_systemCombo, "system");
    addCond(m_studioCombo, "studio");
    addCond(m_vidCombo, "vid");
    addCond(m_sourceCombo, "source");

    if (m_statusCombo && m_statusCombo->currentIndex() > 0) {
        if (m_statusCombo->currentText() == QStringLiteral("Без статуса")) {
            conds << QStringLiteral("(perevod IS NULL OR perevod = '')");
        } else {
            conds << QStringLiteral("perevod = ?");
            values->append(m_statusCombo->currentText());
        }
    }

    return conds.isEmpty() ? QString() : conds.join(QStringLiteral(" AND "));
}

void MainWindow::refreshQuery()
{
    if (!m_model) {
        m_model = makeModel(this);
        m_table->setModel(m_model);
        connect(m_table->selectionModel(), &QItemSelectionModel::currentRowChanged,
                this, &MainWindow::onSelectionChanged);
    }

    if (!QSqlDatabase::contains(QStringLiteral("book"))) {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("book"));
        db.setDatabaseName(m_dbPath);
        if (!db.open()) {
            m_statusLabel->setText(QStringLiteral("Ошибка открытия БД: ") +
                                   db.lastError().text());
            return;
        }
    }

    QStringList values;
    const QString where = buildWhereClause(&values);

    QString sql = QStringLiteral(
        "SELECT [index], name_book, ahtor, yaer, format, lang_string, pages, "
        "size, source, izdatel, part, image, perevod FROM book");
    if (!where.isEmpty())
        sql += QStringLiteral(" WHERE ") + where;
    sql += QStringLiteral(" ORDER BY [index]");

    QSqlQuery query(QSqlDatabase::database(QStringLiteral("book")));
    query.prepare(sql);
    for (int i = 0; i < values.size(); ++i)
        query.addBindValue(values.at(i));
    if (!query.exec()) {
        m_statusLabel->setText(QStringLiteral("Ошибка запроса: ") +
                               query.lastError().text());
        return;
    }
    m_model->setQuery(query);
    applyHeaders(m_model);

    QHeaderView *header = m_table->horizontalHeader();
    header->moveSection(8, 6);
    header->moveSection(12, 7);   // perevod -> Статус
    m_table->setColumnHidden(4, true);  // format
    m_table->setColumnHidden(7, true);  // size
    m_table->setColumnHidden(9, true);  // izdatel
    m_table->setColumnHidden(10, true); // part
    m_table->setColumnHidden(11, true); // image

    m_statusLabel->setText(tr("Книга %1 из %2")
                               .arg(m_model->rowCount() > 0 ? 1 : 0)
                               .arg(m_model->rowCount()));
    m_detailsBrowser->clear();
    m_coverLabel->setText(QStringLiteral("Нет обложки"));

    if (m_model->rowCount() > 0) {
        m_table->setCurrentIndex(m_model->index(0, 0));
        showBookDetails(0);
    }
}

void MainWindow::onSelectionChanged(const QModelIndex &current)
{
    showBookDetails(current.row());
}

void MainWindow::showBookDetails(int row)
{
    if (!m_model || row < 0 || row >= m_model->rowCount()) {
        m_detailsBrowser->clear();
        m_currentCoverPath.clear();
        m_currentRow = -1;
        m_coverLabel->setText(QStringLiteral("Нет обложки"));
        m_statusLabel->setText(tr("Найдено книг: %1").arg(m_model ? m_model->rowCount() : 0));
        return;
    }

    m_statusLabel->setText(tr("Книга %1 из %2")
                               .arg(row + 1)
                               .arg(m_model->rowCount()));

    const auto at = [this, row](int col) {
        return m_model->index(row, col).data().toString();
    };

    const int id = m_model->index(row, 0).data().toInt();
    const QString name = at(1);
    const QString author = at(2);
    const QString year = at(3);
    const QString format = at(4);
    const QString lang = at(5);
    const QString pages = at(6);
    const QString size = at(7);
    const QString source = at(8);
    const QString izdatel = at(9);
    const QString part = at(10);
    const QString image = at(11);

    const QString dir = bookDir(part);

    QString html = QStringLiteral("<h2>%1</h2>").arg(name.toHtmlEscaped());
    if (!author.isEmpty())
        html += QStringLiteral("<p><b>Автор:</b> %1</p>").arg(author.toHtmlEscaped());
    if (!year.isEmpty())
        html += QStringLiteral("<p><b>Год:</b> %1</p>").arg(year.toHtmlEscaped());
    if (!format.isEmpty())
        html += QStringLiteral("<p><b>Формат:</b> %1</p>").arg(format.toHtmlEscaped());
    if (!lang.isEmpty())
        html += QStringLiteral("<p><b>Язык:</b> %1</p>").arg(lang.toHtmlEscaped());
    if (!pages.isEmpty())
        html += QStringLiteral("<p><b>Страниц:</b> %1</p>").arg(pages.toHtmlEscaped());
    if (!size.isEmpty())
        html += QStringLiteral("<p><b>Размер:</b> %1</p>").arg(size.toHtmlEscaped());
    if (!source.isEmpty())
        html += QStringLiteral("<p><b>Наличие:</b> %1</p>").arg(source.toHtmlEscaped());
    if (!izdatel.isEmpty())
        html += QStringLiteral("<p><b>Издатель:</b> %1</p>").arg(izdatel.toHtmlEscaped());

    const QString descPath = QDir(dir).filePath(QStringLiteral("descripter.txt"));
    QFile desc(descPath);
    if (desc.open(QIODevice::ReadOnly)) {
        const QByteArray raw = desc.readAll();
        QString text = QString::fromUtf8(raw);
        if (text.contains(QChar::ReplacementCharacter)) {
            if (QTextCodec *codec = QTextCodec::codecForName("Windows-1251"))
                text = codec->toUnicode(raw);
        }
        if (!text.trimmed().isEmpty())
            html += QStringLiteral("<hr><p><b>Описание:</b></p><p>%1</p>")
                        .arg(text.toHtmlEscaped().replace(QLatin1Char('\n'),
                                                          QStringLiteral("<br>")));
    }

    m_detailsBrowser->setHtml(html);

    QString coverName = image.trimmed();
    if (coverName.isEmpty())
        coverName = QStringLiteral("cover.jpg");
    QString coverPath = QDir(dir).filePath(coverName);
    if (!QFileInfo::exists(coverPath) && !m_noImagePath.isEmpty())
        coverPath = m_noImagePath;

    m_currentCoverPath = coverPath;
    m_currentRow = row;
    updateCoverPixmap();
}

void MainWindow::updateCoverPixmap()
{
    QPixmap pix;
    if (!m_currentCoverPath.isEmpty() && pix.load(m_currentCoverPath)) {
        const int maxW = qRound(260 * m_coverZoom);
        const int maxH = qRound(340 * m_coverZoom);
        m_coverLabel->setPixmap(pix.scaled(maxW, maxH, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
    } else {
        m_coverLabel->setText(QStringLiteral("Нет обложки"));
    }
}

void MainWindow::prevCover()
{
    if (!m_table || !m_model) return;
    const int row = m_table->currentIndex().row();
    if (row > 0)
        m_table->setCurrentIndex(m_model->index(row - 1, 0));
}

void MainWindow::nextCover()
{
    if (!m_table || !m_model) return;
    const int row = m_table->currentIndex().row();
    if (row < m_model->rowCount() - 1)
        m_table->setCurrentIndex(m_model->index(row + 1, 0));
}

void MainWindow::zoomIn()
{
    if (m_coverZoom < 5.0) {
        m_coverZoom += 0.25;
        updateCoverPixmap();
    }
}

void MainWindow::zoomOut()
{
    if (m_coverZoom > 0.25) {
        m_coverZoom -= 0.25;
        updateCoverPixmap();
    }
}

void MainWindow::addBook()
{
    AddBookDialog dlg(this);
    dlg.setBaseDir(resolveBaseDir());
    dlg.setNoImagePath(m_noImagePath);
    dlg.setFileFilters(m_config.values(QStringLiteral("files")));
    dlg.setReferenceData(m_config.values(QStringLiteral("Format")),
                         m_config.values(QStringLiteral("Lang")),
                         m_config.values(QStringLiteral("Tema")),
                         m_config.values(QStringLiteral("System")),
                         m_config.values(QStringLiteral("Studio")),
                         m_config.values(QStringLiteral("Tipog")),
                         m_config.values(QStringLiteral("Application")));
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString error = dlg.commit();
    if (!error.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка"), error);
        return;
    }
    refreshQuery();
}

void MainWindow::editBook()
{
    if (!m_model || !m_table->currentIndex().isValid())
        return;
    const int row = m_table->currentIndex().row();
    const int id = m_model->index(row, 0).data().toInt();

    EditBookDialog dlg(this);
    dlg.setBaseDir(resolveBaseDir());
    dlg.setBookIndex(id);
    dlg.setFileFilters(m_config.values(QStringLiteral("files")));
    dlg.setReferenceData(m_config.values(QStringLiteral("Format")),
                         m_config.values(QStringLiteral("Lang")),
                         m_config.values(QStringLiteral("Tema")),
                         m_config.values(QStringLiteral("System")),
                         m_config.values(QStringLiteral("Studio")),
                         m_config.values(QStringLiteral("Tipog")),
                         m_config.values(QStringLiteral("Application")));
    dlg.loadBook();
    if (dlg.exec() != QDialog::Accepted)
        return;

    const QString error = dlg.commit();
    if (!error.isEmpty()) {
        QMessageBox::critical(this, QStringLiteral("Ошибка"), error);
        return;
    }
    refreshQuery();
}

void MainWindow::openBook()
{
    if (!m_table->currentIndex().isValid())
        return;
    const int row = m_table->currentIndex().row();
    const auto at = [this, row](int col) {
        return m_model->index(row, col).data().toString();
    };

    const int id = m_model->index(row, 0).data().toInt();
    const QString part = at(10);
    const QString ahtor = at(2);
    const QString format = at(4);

    const QString file = bookFile(part, ahtor, format);
    if (!file.isEmpty() && QFileInfo::exists(file)) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(file))) {
            QMessageBox::warning(this, QStringLiteral("Открыть книгу"),
                                 QStringLiteral("Не удалось открыть файл:\n%1").arg(file));
        }
        return;
    }

    const QString dir = bookDir(part);
    if (!dir.isEmpty() && QDir(dir).exists()) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
            QMessageBox::warning(this, QStringLiteral("Открыть книгу"),
                                 QStringLiteral("Не удалось открыть папку:\n%1").arg(dir));
        }
        return;
    }

    QMessageBox::information(this, QStringLiteral("Открыть книгу"),
                             QStringLiteral("Книга (ID %1) не найдена:\n%2")
                                 .arg(id)
                                 .arg(QDir(dir).absolutePath()));
}

void MainWindow::openBookFolder()
{
    if (!m_table->currentIndex().isValid())
        return;
    const int row = m_table->currentIndex().row();
    const auto at = [this, row](int col) {
        return m_model->index(row, col).data().toString();
    };

    const int id = m_model->index(row, 0).data().toInt();
    const QString part = at(10);

    const QString dir = bookDir(part);
    if (!dir.isEmpty() && QDir(dir).exists()) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) {
            QMessageBox::warning(this, QStringLiteral("Открыть папку"),
                                 QStringLiteral("Не удалось открыть папку:\n%1").arg(dir));
        }
        return;
    }

    QMessageBox::information(this, QStringLiteral("Открыть папку"),
                             QStringLiteral("Книга (ID %1) не найдена:\n%2")
                                 .arg(id)
                                 .arg(QDir(dir).absolutePath()));
}

void MainWindow::resetFilters()
{
    m_searchEdit->clear();
    const QList<QComboBox *> combos = {m_formatCombo, m_langCombo, m_temaCombo,
                                       m_systemCombo, m_studioCombo, m_vidCombo,
                                       m_sourceCombo, m_statusCombo};
    for (QComboBox *c : combos) {
        if (c)
            c->setCurrentIndex(0);
    }
    refreshQuery();
}

void MainWindow::pickRandomBook()
{
    if (!m_model || m_model->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("Что почитать?"),
                                 QStringLiteral("По заданному фильтру книг не найдено."));
        return;
    }

    const int row = QRandomGenerator::global()->bounded(m_model->rowCount());
    m_table->setCurrentIndex(m_model->index(row, 0));
    m_table->scrollTo(m_model->index(row, 0));
    showBookDetails(row);

    const int id = m_model->index(row, 0).data().toInt();
    const QString name = m_model->index(row, 1).data().toString();

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Что почитать?"));
    box.setText(QStringLiteral("Книга (ID %1): %2\n\nЧто с ней делать?")
                    .arg(id)
                    .arg(name));
    QPushButton *readButton =
        box.addButton(QStringLiteral("Читаю"), QMessageBox::AcceptRole);
    QPushButton *translateButton =
        box.addButton(QStringLiteral("Перевожу"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Отмена"), QMessageBox::RejectRole);
    box.exec();

    QString status;
    if (box.clickedButton() == readButton)
        status = QStringLiteral("Читаю");
    else if (box.clickedButton() == translateButton)
        status = QStringLiteral("Перевожу");
    else
        return;

    QSqlQuery query(QSqlDatabase::database(QStringLiteral("book")));
    query.prepare(QStringLiteral("UPDATE book SET perevod = ? WHERE [index] = ?"));
    query.addBindValue(status);
    query.addBindValue(id);
    if (!query.exec()) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось отметить книгу:\n%1")
                                 .arg(query.lastError().text()));
        return;
    }
    refreshQuery();
    m_table->setCurrentIndex(m_model->index(row, 0));
    showBookDetails(row);
}

void MainWindow::clearStatus()
{
    if (!m_table->currentIndex().isValid())
        return;
    const int row = m_table->currentIndex().row();
    const int id = m_model->index(row, 0).data().toInt();
    const QString status = m_model->index(row, 12).data().toString();
    if (status.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Сбросить статус"),
                                 QStringLiteral("У выбранной книги нет статуса."));
        return;
    }

    if (QMessageBox::question(this, QStringLiteral("Сбросить статус"),
                              QStringLiteral("Снять статус «%1» с книги (ID %2)?")
                                  .arg(status)
                                  .arg(id),
                              QMessageBox::Yes | QMessageBox::No)
        != QMessageBox::Yes)
        return;

    QSqlQuery query(QSqlDatabase::database(QStringLiteral("book")));
    query.prepare(QStringLiteral("UPDATE book SET perevod = NULL WHERE [index] = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        QMessageBox::warning(this, QStringLiteral("Ошибка"),
                             QStringLiteral("Не удалось снять статус:\n%1")
                                 .arg(query.lastError().text()));
        return;
    }
    refreshQuery();
}
