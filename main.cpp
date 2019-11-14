#include "liquidmainwindow.h"
#include <QApplication>
#include <QTranslator>
#include <QLibraryInfo>
#include <QFileInfo>
#include "config.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("Herewe");
    a.setOrganizationDomain("Servebeer");
    a.setApplicationName("QLiquidSFZ");

    QString locale = QLocale::system().name();
    QTranslator qtTranslator;
    qtTranslator.load("qt_" + locale,
            QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    a.installTranslator(&qtTranslator);

    QTranslator qliquidsfzTranslator;
    if (!qliquidsfzTranslator.load(TARGET "_" + locale, "locale"))
        qliquidsfzTranslator.load(TARGET "_" + locale, DATADIR "/" TARGET "/locale" );
    a.installTranslator(&qliquidsfzTranslator);

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

    return a.exec();
}
