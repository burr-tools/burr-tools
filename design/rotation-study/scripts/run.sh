#!/bin/sh
# usage: run.sh <burrTxt> <outdir>   (run from the repository root)
B=$1; O=$2; mkdir -p $O
for p in AAA_most_rotations AlPackino CubeInCage PelikanBurr DraculasDentalDesaster BallRoom; do
  for m in 0.081,0 0.0126,0.0317; do
    s=$(date +%s.%N)
    timeout 300 $B -d -R -m $m examples/$p.xmpuzzle > $O/$p.$m.txt 2>&1; rc=$?
    echo "$p $m rc=$rc $(echo "$(date +%s.%N) - $s" | bc)s" >> $O/times.txt
  done
done
