FROM gcr.io/oss-fuzz-base/base-builder

RUN apt-get update && apt-get install -y cmake make

COPY . $SRC/cmark
WORKDIR $SRC/cmark

COPY .clusterfuzzlite/build.sh $SRC/build.sh
