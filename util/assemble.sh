#!/bin/sh

series="$1"
mkdir -p final/$series
for bucket in `cat "$series"-buckets.txt`; do
    ~/geo/ztopo/util/compress.py $series-$bucket.idx 
    cp $series-$bucket.idxz final/$series/$bucket.idxz
    cat $series-$bucket.lst | xargs cat >> final/$series/$bucket.dat
done
