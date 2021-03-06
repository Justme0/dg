#!/bin/bash

M="$1"

llvm-dis "$M" -o __m.ll
llvm-dis "${M%%.bc}.sliced" -o __m.sliced.ll

CMD=diff
if which vimdiff &>/dev/null; then # prefer vimdiff 2016.7.3 jiangg
	CMD=vimdiff
elif which meld &>/dev/null; then
	CMD=meld
elif which kompare &>/dev/null; then
	CMD=kompare
fi

$CMD __m.ll __m.sliced.ll
rm __m.ll __m.sliced.ll

