TEMPLATE = app
TARGET = ForzaPainterFilter
QT += core gui widgets

HEADERS += ./ellipse.h \
    ./Simplify.h \
    ./Viewer.h
SOURCES += ./main.cpp \
    ./Simplify.cpp \
    ./Viewer.cpp
FORMS += ./Simplify.ui

CONFIG += c++17