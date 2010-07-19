
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread 
# release

# Input
HEADERS += consts.h mainwindow.h map.h mapprojection.h maprenderer.h \
	mapwidget.h projection.h
SOURCES += main.cpp \
           mainwindow.cpp \
           map.cpp \
           mapprojection.cpp \
	   maprenderer.cpp \
	   mapwidget.cpp projection.cpp
QT += opengl
INCLUDEPATH += /opt/local/include
LIBS += -L/opt/local/lib -lproj
