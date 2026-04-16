FROM gcr.io/oss-fuzz-base/base-builder

RUN apt-get update && apt-get install -y make

COPY . $SRC/json4cpp
WORKDIR $SRC/json4cpp

COPY .clusterfuzzlite/build.sh $SRC/build.sh
