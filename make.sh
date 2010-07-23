#!/bin/sh

export BERKELEY_ROOT="/opt/local/lib/db47"
export CMAKE_OSX_ARCHITECTURES="i386"


cmake . -DBERKELEY_DB_LIBRARIES=/opt/local/lib/db47 -DBERKELEY_DB_INCLUDE_DIR=/opt/local/include/db47

