#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTextCodec>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("book"));
    QApplication::setApplicationName(QStringLiteral("book_app"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/book.png")));
    QLocale::setDefault(QLocale(QLocale::Russian, QLocale::Russia));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    QCoreApplication::addLibraryPath(
        QLibraryInfo::location(QLibraryInfo::PluginsPath));
#endif

    MainWindow w;
    w.show();
    return app.exec();
}
