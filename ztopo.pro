
BOOST_ROOT = "/opt/local" 
PROJ4_ROOT = "/Users/hawkinsp/Documents/Wine/drive_c/p/proj-4.7.0"
PROJ4_LIBS = "$$PROJ4_ROOT/src/.libs"
PROJ4_INCLUDE = "$$PROJ4_ROOT/src"
BDB_ROOT = "/Users/hawkinsp/Documents/Wine/drive_c/p"
BDB_LIB = db-5.0
QJSON_ROOT = "/Users/hawkinsp/Documents/Wine/drive_c/Program Files/qjson"

TEMPLATE = app
TARGET = 
DEPENDPATH += . src 
INCLUDEPATH += . src "$$PROJ4_INCLUDE" "$$BOOST_ROOT/include"
INCLUDEPATH += "$$QJSON_ROOT/include" "$$BDB_ROOT/include"
LIBS += -L "$$BDB_ROOT/lib" -l$$BDB_LIB
LIBS += -L"$$QJSON_ROOT/lib" -lqjson
LIBS += -L"$$PROJ4_LIBS" -lproj


# Input
HEADERS += src/consts.h \
           src/coordformatter.h \
           src/mainwindow.h \
           src/map.h \
           src/maprenderer.h \
           src/mapwidget.h \
           src/preferences.h \
           src/printscene.h \
           src/printview.h \
           src/projection.h \
           src/rootdata.h \
           src/searchhandler.h \
           src/tilecache.h
FORMS += src/preferences.ui
SOURCES += src/coordformatter.cpp \
           src/main.cpp \
           src/mainwindow.cpp \
           src/map.cpp \
           src/maprenderer.cpp \
           src/mapwidget.cpp \
           src/preferences.cpp \
           src/printscene.cpp \
           src/printview.cpp \
           src/projection.cpp \
           src/rootdata.cpp \
           src/searchhandler.cpp \
           src/tilecache.cpp \

RESOURCES += src/resources.qrc
QT += network opengl xml
RC_FILE = images/topo.rc
