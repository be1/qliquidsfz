#include "liquidmainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QFileInfo>
#include <QCommandLineParser>
#include "config.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("Herewe");
    a.setOrganizationDomain("Servebeer");
    a.setApplicationName("QLiquidSFZ");
    a.setApplicationVersion(VERSION " (" REVISION ")");

    QString locale = QLocale::system().name();
    QTranslator qtTranslator;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    (void) qtTranslator.load("qt_" + locale,
            QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#else
    (void) qtTranslator.load("qt_" + locale,
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
#endif
    a.installTranslator(&qtTranslator);

    QTranslator qliquidsfzTranslator;
    if (!qliquidsfzTranslator.load(TARGET "_" + locale, "locale"))
        (void) qliquidsfzTranslator.load(TARGET "_" + locale, DATADIR "/" TARGET "/locale" );
    a.installTranslator(&qliquidsfzTranslator);


    QCommandLineParser parser;
    parser.setApplicationDescription("Simple LiquidSFZ synthesizer GUI.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("sfz", QCoreApplication::translate("main", "SFZ file to load."));

    parser.process(a);
    const QStringList args = parser.positionalArguments();

    LiquidMainWindow w;
    QString iconpath = QString(DATADIR "/pixmaps/" TARGET ".png");
#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    if (QFileInfo::exists(iconpath))
#else
    if ((QFileInfo(iconpath)).exists())
#endif
        w.setWindowIcon(QIcon(iconpath));
    else
        w.setWindowIcon(QIcon(TARGET ".png"));

    w.show();

    if (args.length() > 0)
        w.loadFile(args.at(0));

    return a.exec();
}
