QT += gui widgets
TARGET = TraceView
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    traceview.cpp \
    tracedata.cpp
HEADERS += mainwindow.h \
    traceview.h \
    tracedata.h
FORMS += mainwindow.ui

macx {
    ICON += TraceView.icns
    QMAKE_INFO_PLIST = Info.plist
}
