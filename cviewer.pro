
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread 
# release

# Input
HEADERS += consts.h mainwindow.h map.h mapprojection.h \
	tileiothread.h mapwidget.h
SOURCES += main.cpp \
           mainwindow.cpp \
           map.cpp \
           mapprojection.cpp \
	   mapwidget.cpp \
	   tileiothread.cpp
QT += opengl
LIBS += -L/opt/local/lib -lproj -framework GDAL -lgdal
