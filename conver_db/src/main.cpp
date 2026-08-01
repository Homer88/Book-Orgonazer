#include "converter.h"
#include "mainwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QStringList>

#include <cstdio>

static int runCli(const QString &inPath, const QString &outPath)
{
    Converter converter;
    QObject::connect(&converter, &Converter::logMessage, [](const QString &m) {
        const QByteArray b = m.toUtf8();
        fwrite(b.constData(), 1, b.size(), stdout);
        fputc('\n', stdout);
        fflush(stdout);
    });
    QObject::connect(&converter, &Converter::errorMessage, [](const QString &m) {
        const QByteArray b = m.toUtf8();
        fwrite(b.constData(), 1, b.size(), stderr);
        fputc('\n', stderr);
        fflush(stderr);
    });
    bool ok = false;
    QObject::connect(&converter, &Converter::finished,
                     [&ok](bool result, const QString &) { ok = result; });

    converter.convert(inPath, outPath);
    return ok ? 0 : 1;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    if (args.size() >= 3)
        return runCli(args.at(1), args.at(2));

    MainWindow w;
    if (args.size() == 2) {
        const QFileInfo fi(args.at(1));
        const QString out = fi.absolutePath() + QLatin1Char('/') +
                            fi.completeBaseName() + QStringLiteral(".sqlite");
        w.startConversion(args.at(1), out);
    }
    w.show();
    return app.exec();
}
