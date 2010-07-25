#!/opt/local/bin/python

from PyQt4.QtCore import *
import sys

if len(sys.argv) < 2:
    print "Usage: compress <file> ..."

for file in sys.argv[1:]:
    f = QFile(file)
    f.open(QIODevice.ReadOnly)
    bytes = f.readAll()
    f.close()
    zbytes = qCompress(bytes, 9)
    g = QFile(file + "z")
    g.open(QIODevice.WriteOnly)
    g.write(zbytes)
    g.close()

