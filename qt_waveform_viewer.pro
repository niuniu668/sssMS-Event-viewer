QT += core gui widgets

CONFIG += c++17
CONFIG += no_moc_predefs

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    spectrogramwidget.cpp \
    spectrumwidget.cpp \
    waveformreader.cpp \
    waveformwidget.cpp

HEADERS += \
    mainwindow.h \
    spectrogramwidget.h \
    spectrumwidget.h \
    waveformreader.h \
    waveformwidget.h

TARGET = QtWaveformViewer
TEMPLATE = app
