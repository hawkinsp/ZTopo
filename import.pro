
TEMPLATE = app
TARGET = 
DEPENDPATH += .
INCLUDEPATH += .
CONFIG += qt thread 
# release

# Input
HEADERS += consts.h  map.h mapprojection.h 
SOURCES += import.cpp \
           map.cpp \
           mapprojection.cpp 
LIBS += -L/opt/local/lib -lproj -framework GDAL -lgdal
