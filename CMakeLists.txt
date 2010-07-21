cmake_minimum_required (VERSION 2.6) 
project (topo) 

find_package(Qt4 REQUIRED)

include_directories (/opt/local/include)
include_directories (/Users/hawkinsp/opt/qjson/include)
link_directories (/opt/local/lib /Users/hawkinsp/opt/qjson/lib)


IF(APPLE)
   #SET(GUI_TYPE MACOSX_BUNDLE)
#	   INCLUDE_DIRECTORIES ( /Developer/Headers/FlatCarbon )
   FIND_LIBRARY(COREFOUNDATION_LIBRARY CoreFoundation )
   MARK_AS_ADVANCED (COREFOUNDATION_LIBRARY)
   SET(EXTRA_LIBS ${COREFOUNDATION_LIBRARY})
ENDIF (APPLE)

add_subdirectory (src) 
