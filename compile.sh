#!/usr/bin/env sh

# This is a temporary script (before moving to make)

mkdir -p out
gcc src/libtl.c src/libtlaux.c src/libtlstd.c src/tli.c src/libtlht.c -fsanitize=address -m32 -o out/tli
