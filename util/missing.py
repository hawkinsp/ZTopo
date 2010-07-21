#!/usr/bin/python

# Identify tile prefixes that are missing from a tile set.

import sys

maxLevel = int(sys.argv[1])

map = {}
for line in sys.stdin.readlines():
    i = 1
    for c in line[:-1]:
        x = ord(c) - ord("0")
        assert x >= 0 and x <= 3
        i = i << 2 | x
    map[i] = 1

def tostr(x):
    y = ""
    while x > 1:
        y = chr(ord("0") + (x & 3)) + y
        x = x >> 2
    return y

def find(x, len):
    if map.has_key(x):
        if len < maxLevel:
            for i in range(0, 4):
                find((x << 2) | i, len + 1)
    else:
        print tostr(x)

find(1, 0)
