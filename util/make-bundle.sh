#!/bin/sh
BUNDLE=build/src/ztopo.app
CONTENTS="$BUNDLE/Contents"
RESOURCES="$BUNDLE/Contents/Resources"
FRAMEWORKS="$BUNDLE/Contents/Frameworks"
mkdir -p $RESOURCES $FRAMEWORKS $RESOURCES/proj4
cp config/Info.plist $CONTENTS/ 
cp images/topo.icns $RESOURCES/
cp /Users/hawkinsp/geo/proj-datum/* $RESOURCES/proj4/

