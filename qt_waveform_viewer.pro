QT += core gui widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    waveformreader.cpp \
    waveformwidget.cpp

HEADERS += \
    mainwindow.h \
    waveformreader.h \
    waveformwidget.h

TARGET = QtWaveformViewer
TEMPLATE = app
