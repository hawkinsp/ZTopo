
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread 
# release

# Input
HEADERS += consts.h mainwindow.h map.h mapprojection.h maprenderer.h \
	mapwidget.h
SOURCES += main.cpp \
           mainwindow.cpp \
           map.cpp \
           mapprojection.cpp \
	   maprenderer.cpp \
	   mapwidget.cpp 
QT += opengl
LIBS += -L/Users/hawkinsp/opt/gdal-1.7.2/lib -lgdal
#LIBS += -L/opt/local/lib -lproj -framework GDAL -lgdal
