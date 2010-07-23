set(QT_USE_OPENGL TRUE)
set(QT_USE_NETWORK TRUE)
include(${QT_USE_FILE})
set(common_SRCS
  projection.cpp
  rootdata.cpp
  map.cpp
  prefix.cpp 
) 
set(ztopo_SRCS
  coordformatter.cpp 
  main.cpp
  mainwindow.cpp
  maprenderer.cpp
  mapwidget.cpp
  tilecache.cpp
  ${common_SRCS})
set(ztopo_MOC_HDRS
  mainwindow.h
  maprenderer.h
  mapwidget.h
  tilecache.h
)
set(ztopo_RCCS
  images.qrc
)  
set(import_SRCS
  import.cpp
  ${common_SRCS})
set(merge_SRCS
  merge.cpp
  ${common_SRCS})

QT4_ADD_RESOURCES(ztopo_RCC_SRCS ${ztopo_RCCS})
QT4_WRAP_CPP(ztopo_MOC_SRCS ${ztopo_MOC_HDRS})

add_executable(ztopo MACOSX_BUNDLE ${ztopo_SRCS} ${ztopo_MOC_SRCS} ${ztopo_RCC_SRCS})
add_executable(import ${import_SRCS})
add_executable(merge ${merge_SRCS})

target_link_libraries(ztopo 
  ${QT_LIBRARIES} qjson ${EXTRA_LIBS} ${QT_QTOPENGL_LIBRARIES}
  ${QT_QTNETWORK_LIBRARIES}
  /opt/local/lib/libproj.a ${Boost_Libraries} db_cxx)
target_link_libraries(import ${QT_LIBRARIES} proj ${GDAL_LIBRARY} qjson)
target_link_libraries(merge ${QT_LIBRARIES} proj qjson)
