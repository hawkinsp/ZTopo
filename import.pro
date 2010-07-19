
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread 
# release

# Input
HEADERS += consts.h  map.h mapprojection.h  projection.h
SOURCES += import.cpp \
           map.cpp \
           mapprojection.cpp projection.cpp
INCLUDEPATH += /opt/local/include
LIBS += -L/opt/local/lib -lproj -framework GDAL -lgdal
