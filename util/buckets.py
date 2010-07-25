#!/usr/bin/python

import sys
import array

if len(sys.argv) < 4:
    print "Usage: bucket <series> <max level> <index step>"
    sys.exit(1)

series = sys.argv[1]
maxLevel = int(sys.argv[2])
idxStep = int(sys.argv[3])

tile_sizes = {}

def quadKeyStrToInt(s):
    q = 1
    for i in range(len(s) - 1, -1, -1):
        c = ord(s[i]) - ord("0")
        q = (q << 2) | c
    return q

# Read size/key pairs
for line in sys.stdin.readlines():
    i = 1
    size, key = line[:-1].split(" ")
    tile_sizes[key] = int(size)

# Break the tree up into buckets
prefix_sizes = {}
for key in tile_sizes:
    p = ((len(key) - 1) / idxStep) * idxStep
    prefix = key[:p]
    suffix = key[p:]
    if not prefix_sizes.has_key(prefix):
        prefix_sizes[prefix] = {"": 0}
    prefix_sizes[prefix][suffix] = tile_sizes[key]

# Sum the tile tree sizes in each bucket
for prefix in prefix_sizes:
    suffixes = [(len(x), x) for x in prefix_sizes[prefix].keys()]
    suffixes.sort(None, None, True)
    for (n, suffix) in suffixes:
        if n != 0:
            prefix_sizes[prefix][suffix[:-1]] = prefix_sizes[prefix][suffix[:-1]] + prefix_sizes[prefix][suffix]
            

def create_bucket(sizes, idx, tile_list):
    def visit(key, off):
        if not sizes.has_key(key):
            return

        for i in range(0, 4):
            c = chr(ord("0") + i)
            keyc = key + c
            if sizes.has_key(keyc):
                idx[off + i] = sizes[keyc]
                tile_list.append(keyc)
                
        for i in range(0, 4):
            c = chr(ord("0") + i)
            visit(key + c, (off + i + 1) * 4)

    visit("", 0)


for prefix in prefix_sizes:
    print "t" + prefix
    n = len(prefix)
    next_n = min(n + idxStep, maxLevel)
    numEntries = (4**(next_n - n + 1) -1)/3 - 1
    idx = array.array("I", [0]*numEntries)
    assert(idx.itemsize == 4)
    tile_list = []
    create_bucket(prefix_sizes[prefix], idx, tile_list)
    f = open(series + "-t" + prefix + ".idx", "w")
    idx.tofile(f)
    f.close()
    g = open(series + "-t" + prefix + ".txt", "w")
    for tile in tile_list:
        pt = prefix + tile
        name = series
        l = len(pt)
        for x in range(0, l, 3):
            name = name + "/" + pt[x:min(x+3,l)]
        g.write(name + "t.png\n")
    g.close()
