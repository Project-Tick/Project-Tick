#!/bin/bash -eu

cd $SRC/cmark
mkdir -p build && cd build
cmake .. -DCMARK_TESTS=OFF -DCMARK_STATIC=ON
make -j$(nproc)

$CC $CFLAGS -I$SRC/cmark/src -I$SRC/cmark/build/src \
    $SRC/cmark/fuzz/cmark-fuzz.c -o $OUT/cmark-fuzz \
    $SRC/cmark/build/src/libcmark.a \
    $LIB_FUZZING_ENGINE

cp $SRC/cmark/fuzz/dictionary $OUT/cmark-fuzz.dict
