#!/bin/sh
BUNDLE=topoviewer.app
RESOURCES="$BUNDLE/Contents/Resources"
FRAMEWORKS="$BUNDLE/Contents/Frameworks"
mkdir -p $RESOURCES $FRAMEWORKS $RESOURCES/proj4
cp ../config/root.json $RESOURCES/ 
cp /Users/hawkinsp/geo/proj-datum/* $RESOURCES/proj4/
