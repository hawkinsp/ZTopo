cmake_minimum_required (VERSION 2.6) 
project (ztopo) 

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules/")
find_package(Qt4 REQUIRED)
find_package(BerkeleyDB REQUIRED)
find_package(Boost 1.36 REQUIRED)
find_package(GDAL REQUIRED)

SET(CMAKE_CXX_FLAGS "-Wall")

include_directories (${Boost_INCLUDE_DIRS})
include_directories(${QT_QTOPENGL_INCLUDE_DIR})
include_directories(${QT_QTNETWORK_INCLUDE_DIR})
include_directories(${GDAL_INCLUDE_DIR})
include_directories(${BERKELEY_DB_INCLUDE_DIR})

include_directories (/opt/local/include)
#include_directories (/Users/hawkinsp/opt/gdal-1.7.2/include)
include_directories (/Users/hawkinsp/opt/qjson/include)

#link_directories (/Users/hawkinsp/opt/gdal-1.7.2/lib)
link_directories(/opt/local/lib /Users/hawkinsp/opt/qjson/lib)
link_directories(${BERKELEY_DB_LIBRARIES})
IF(APPLE)
   #SET(GUI_TYPE MACOSX_BUNDLE)
#	   INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
   FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation )
   MARK_AS_ADVANCED (COREFOUNDATION_LIBRARY)
   SET(EXTRA_LIBS ${COREFOUNDATION_LIBRARY})
ENDIF (APPLE)

add_subdirectory (src) 
