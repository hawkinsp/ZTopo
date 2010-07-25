#!/bin/sh

export BERKELEY_ROOT="/opt/local/lib/db47"
export CMAKE_OSX_ARCHITECTURES="i386"
export GDAL_ROOT="/Users/hawkinsp/opt/gdal-1.7.2"


cmake .. -DBERKELEY_DB_LIBRARIES=/opt/local/lib/db47 -DBERKELEY_DB_INCLUDE_DIR=/opt/local/include/db47 -DCMAKE_BUILD_TYPE=Debug

