#-------------------------------------------------
#
# Project created by QtCreator 2019-11-13T01:01:15
#
#-------------------------------------------------

QT        += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

VERSION = 0.1.0
TARGET = qliquidsfz
TEMPLATE = app

unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += liquidsfz jack
    isEmpty(PREFIX): PREFIX = /usr/local
    isEmpty(BINDIR): BINDIR = $$PREFIX/bin
    isEmpty(DATADIR): DATADIR = $$PREFIX/share
}


config.input = config.h.in
config.output = config.h
QMAKE_SUBSTITUTES += config

isEmpty(QMAKE_LRELEASE):QMAKE_LRELEASE = $$[QT_INSTALL_BINS]/lrelease

TRANSLATIONS += \
    qliquidsfz_en.ts \
    qliquidsfz_fr.ts

LOCALE_DIR = locale

updateqm.input = TRANSLATIONS
updateqm.output = $$LOCALE_DIR/${QMAKE_FILE_BASE}.qm
updateqm.commands = $$QMAKE_LRELEASE ${QMAKE_FILE_IN} -qm $$LOCALE_DIR/${QMAKE_FILE_BASE}.qm
updateqm.CONFIG += no_link target_predeps
QMAKE_EXTRA_COMPILERS += updateqm

unix {
    MANPAGE = "qliquidsfz.1"
    manpage.input = MANPAGE
    manpage.output = $${MANPAGE}.gz
    manpage.commands = gzip --to-stdout ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}
    manpage.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += manpage
}

unix {
    target.path = $$BINDIR
    manual.path = $$DATADIR/man/man1
    manual.files = $${MANPAGE}.gz
    manual.CONFIG = no_check_exist
    translations.path = $$DATADIR/$${TARGET}
    translations.files = $${LOCALE_DIR}
    desktop.path = $$DATADIR/applications
    desktop.files = $${TARGET}.desktop
    icon.path = $$DATADIR/pixmaps
    icon.files = qliquidsfz.png
    INSTALLS += target \
            manual \
            translations \
            desktop \
            icon
}


# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0


SOURCES += \
        main.cpp \
        liquidmainwindow.cpp \
    sfzloader.cpp

HEADERS += \
        liquidmainwindow.h \
    sfzloader.h

FORMS += \
        liquidmainwindow.ui
