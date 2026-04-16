#!/bin/bash -eu

cd $SRC/json4cpp

for fuzzer in parse_json parse_bson parse_cbor parse_msgpack parse_ubjson parse_bjdata; do
    $CXX $CXXFLAGS -std=c++11 -I single_include \
        tests/src/fuzzer-${fuzzer}.cpp \
        -o $OUT/${fuzzer}_fuzzer \
        $LIB_FUZZING_ENGINE
done
