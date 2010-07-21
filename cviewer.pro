
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread 
# release

# Input
HEADERS += consts.h mainwindow.h map.h mapprojection.h maprenderer.h \
	mapwidget.h projection.h coordformatter.h prefix.h
SOURCES += main.cpp \
           mainwindow.cpp \
           map.cpp \
           mapprojection.cpp \
	   maprenderer.cpp \
	   mapwidget.cpp projection.cpp coordformatter.cpp prefix.cpp
RESOURCES += images.qrc
QT += opengl
INCLUDEPATH += /opt/local/include
LIBS += -L/opt/local/lib -lproj
