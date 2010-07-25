#!/bin/sh

series="$1"
for bucket in `cat "$series"-buckets.txt`; do
    cp $series-$bucket.idx final/
    cat $series-$bucket.txt | xargs cat >> final/$series-$bucket.dat
done
