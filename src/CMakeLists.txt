set(QT_USE_OPENGL TRUE)
include(${QT_USE_FILE})
set(common_SRCS
  projection.cpp
  rootdata.cpp
  map.cpp
  prefix.cpp 
) 
set(topoviewer_SRCS
  coordformatter.cpp 
  main.cpp
  mainwindow.cpp
  maprenderer.cpp
  mapwidget.cpp
  ${common_SRCS})
set(topoviewer_MOC_HDRS
  mainwindow.h
  maprenderer.h
  mapwidget.h
)
set(topoviewer_RCCS
  images.qrc
)  
set(import_SRCS
  import.cpp
  ${common_SRCS})
set(merge_SRCS
  merge.cpp
  ${common_SRCS})

include_directories(${QT_QTOPENGL_INCLUDE_DIR})

QT4_ADD_RESOURCES(topoviewer_RCC_SRCS ${topoviewer_RCCS})
QT4_WRAP_CPP(topoviewer_MOC_SRCS ${topoviewer_MOC_HDRS})

add_executable(topoviewer MACOSX_BUNDLE ${topoviewer_SRCS} ${topoviewer_MOC_SRCS} ${topoviewer_RCC_SRCS})
add_executable(import ${import_SRCS})
add_executable(merge ${merge_SRCS})

target_link_libraries(topoviewer ${QT_LIBRARIES} qjson ${EXTRA_LIBS} ${QT_QTOPENGL_LIBRARIES} /opt/local/lib/libproj.a)
target_link_libraries(import ${QT_LIBRARIES} proj gdal qjson)
target_link_libraries(merge ${QT_LIBRARIES} proj qjson)
