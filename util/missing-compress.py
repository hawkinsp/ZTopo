#!/opt/local/bin/python

import sys
from PyQt4.QtCore import *

f = QFile(sys.argv[1])
f.open(QIODevice.ReadOnly)
data = f.readAll()
f.close()

cdata = qCompress(data, 9)

g = QFile(sys.argv[2])
g.open(QIODevice.WriteOnly)
g.write(cdata)
g.close()
