
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread

# Input
HEADERS += consts.h mainwindow.h map.h mapprojection.h mapscene.h mapview.h \
	tileiothread.h
SOURCES += main.cpp \
           mainwindow.cpp \
           map.cpp \
           mapprojection.cpp \
           mapscene.cpp \
           mapview.cpp \
	   tileiothread.cpp
QT += opengl
# LIBS += -lgdal
