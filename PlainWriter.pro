#-------------------------------------------------
#
# Project created by QtCreator 2020-05-26T12:18:11
#
#-------------------------------------------------

QT      += core gui
QT      += xml
QT      += sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = PlainWriter
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++11
CONFIG += exceptions

ICON = Icon.icns

SOURCES += \
        common.cpp \
        confighost.cpp \
        dbaccess.cpp \
        main.cpp \
        mainframe.cpp \
        novelhost.cpp

HEADERS += \
        common.h \
        confighost.h \
        dbaccess.h \
        mainframe.h \
        novelhost.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    image.qrc

DISTFILES += \
    README.md

FORMS +=
