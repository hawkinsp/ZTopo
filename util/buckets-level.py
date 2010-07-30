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

def get_bucket(n):
    if n > idxStep:
        return idxStep
    else:
        return 0

def bucket_size(p):
    if p == 0:
        return idxStep
    else:
        return maxLevel - idxStep

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
    p = get_bucket(len(key))
#    p = ((len(key) - 1) / idxStep) * idxStep

    prefix = key[:p]
    suffix = key[p:]
    n = len(suffix)
    if not prefix_sizes.has_key(prefix):
        numLevels = bucket_size(p)
#        numLevels = min(idxStep, maxLevel - p)
        prefix_sizes[prefix] = [{} for i in range(0, numLevels + 1)]
    prefix_sizes[prefix][n][suffix] = tile_sizes[key]

# Sum the tile tree sizes in each bucket for each level
for prefix in prefix_sizes:
    numLevels = bucket_size(len(prefix))
    # numLevels = min(idxStep, maxLevel - len(prefix))
    for n in range(numLevels, 0, -1):
        for suffix in prefix_sizes[prefix][n].keys():
            for m in range(n, numLevels + 1):
                sub = suffix[:-1]
                prefix_sizes[prefix][m][sub] = prefix_sizes[prefix][m].get(sub, 0) + prefix_sizes[prefix][m][suffix]

# Print out bucket sizes
#for prefix in prefix_sizes.keys():
#    total = 0
#    for t in prefix_sizes[prefix]:
#        for c in ["0", "1", "2", "3"]:
#            total = total + t.get(c, 0)
#    print total, prefix_sizes[prefix]
#sys.exit(0)

def find_offset(key):
    l = len(key)
    if l == 0:
        return 0
    pos = 0
    for i in range(0, l - 1):
        pos = 4 * (pos + 1 + ord(key[i]) - ord("0"))
    return pos + ord(key[l - 1]) - ord("0") + 1
        

# Write out each index
b = open(series + "-buckets.txt", "w")

for prefix in prefix_sizes:
    b.write("t" + prefix + "\n")
    numLevels = bucket_size(len(prefix))
    numEntries = sum([(4**(i + 1) - 1)/3 for i in range(1, numLevels + 1)])
    idx = array.array("I", [0]*numEntries)
    assert(idx.itemsize == 4)
    base = 0
    g = open(series + "-t" + prefix + ".lst", "w")
    for n in range(1, numLevels + 1):
        ts = prefix_sizes[prefix][n]
        for t in ts.keys():
            off = find_offset(t)
            #print n, base, off
            assert (idx[base + off] == 0)
            idx[base + off] = ts[t]
        tiles = [t for t in ts if len(t) == n]
        tiles.sort()
        for tile in tiles:
            pt = prefix + tile
            name = series
            l = len(pt)
            for x in range(0, l, 3):
                name = name + "/" + pt[x:min(x+3,l)]
            g.write(name + "t.png\n")
        #print prefix, n, base, tiles
        base = base + (4**(n + 1) - 1)/3
    f = open(series + "-t" + prefix + ".idx", "w")
    idx.tofile(f)
    f.close()
    g.close()

b.close()

sys.exit(0)

