FROM gcr.io/oss-fuzz-base/base-builder@sha256:d4a4756179c5381cf749fe4e52ca42a3cc8928d353bc7dc8d26acd2ce5f4e80e

RUN apt-get update && apt-get install -y cmake make

COPY . $SRC/project-tick
WORKDIR $SRC/project-tick

COPY .clusterfuzzlite/build.sh $SRC/build.sh
