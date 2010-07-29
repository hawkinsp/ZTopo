TEMPLATE = app
TARGET = 
DEPENDPATH += . src 
INCLUDEPATH += . src /Users/hawkinsp/Documents/Wine/drive_c/p/proj-4.7.0/src /opt/local/include
INCLUDEPATH += "/Users/hawkinsp/Documents/Wine/drive_c/Program Files/qjson/include"
INCLUDEPATH += "/Users/hawkinsp/Documents/Wine/drive_c/Program Files/Oracle/Berkeley DB 11gR2 5.0.26/include"
LIBS += -L/Users/hawkinsp/Documents/Wine/drive_c/p/bdblib -ldb50
#INCLUDEPATH += . src /opt/local/include /Users/hawkinsp/opt/qjson/include /opt/local/include/db47
#LIBS += -L/opt/local/lib/db47 -ldb_cxx-4.7 -framework CoreFoundation -L/Users/hawkinsp/opt/qjson/lib -lqjson -L/opt/local/lib -lproj

# Input
HEADERS += src/consts.h \
           src/coordformatter.h \
           src/mainwindow.h \
           src/map.h \
           src/maprenderer.h \
           src/mapwidget.h \
           src/preferences.h \
           src/projection.h \
           src/rootdata.h \
           src/tilecache.h
FORMS += src/preferences.ui
SOURCES += src/coordformatter.cpp \
           src/main.cpp \
           src/mainwindow.cpp \
           src/map.cpp \
           src/maprenderer.cpp \
           src/mapwidget.cpp \
           src/preferences.cpp \
           src/projection.cpp \
           src/rootdata.cpp \
           src/tilecache.cpp \

RESOURCES += src/images.qrc
QT += network opengl
