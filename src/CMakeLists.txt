set(QT_USE_OPENGL TRUE)
set(QT_USE_NETWORK TRUE)
set(QT_USE_XML TRUE)
include(${QT_USE_FILE})
set(common_SRCS
  projection.cpp
  rootdata.cpp
  map.cpp
) 
set(ztopo_SRCS
  coordformatter.cpp 
  main.cpp
  mainwindow.cpp
  maprenderer.cpp
  mapwidget.cpp
  preferences.cpp
  tilecache.cpp
  ${common_SRCS})
set(ztopo_MOC_HDRS
  mainwindow.h
  maprenderer.h
  mapwidget.h
  tilecache.h
  preferences.h
)
set(ztopo_RCCS
  resources.qrc
)  
set(ztopo_UIS
  preferences.ui
)  
set(import_SRCS
  import.cpp
  ${common_SRCS})
set(merge_SRCS
  merge.cpp
  ${common_SRCS})


INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR})

QT4_ADD_RESOURCES(ztopo_RCC_SRCS ${ztopo_RCCS})
QT4_WRAP_CPP(ztopo_MOC_SRCS ${ztopo_MOC_HDRS})
QT4_WRAP_UI(ztopo_UI_SRCS ${ztopo_UIS})



add_executable(ztopo MACOSX_BUNDLE ${ztopo_SRCS} ${ztopo_MOC_SRCS}
${ztopo_RCC_SRCS} ${ztopo_UI_SRCS})
add_executable(import ${import_SRCS})
add_executable(merge ${merge_SRCS})

target_link_libraries(ztopo 
  ${QT_LIBRARIES} 
  qjson 
  ${EXTRA_LIBS} 
  ${QT_QTOPENGL_LIBRARIES}
  ${QT_QTNETWORK_LIBRARIES}
  ${QT_QTXML_LIBRARIES}
  ${proj_lib}
  ${Boost_Libraries}
  db)
target_link_libraries(import ${QT_LIBRARIES} proj ${GDAL_LIBRARY} qjson)
target_link_libraries(merge ${QT_LIBRARIES} proj qjson)
