#!/bin/bash -eu

cd $SRC/neozip
mkdir -p build && cd build

cmake .. \
    -DBUILD_TESTING=ON \
    -DWITH_FUZZERS=ON \
    -DWITH_GTEST=OFF \
    -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)

for fuzzer in fuzzer_checksum fuzzer_compress fuzzer_example_small \
              fuzzer_example_large fuzzer_example_flush fuzzer_example_dict; do
    cp test/fuzz/$fuzzer $OUT/
done
