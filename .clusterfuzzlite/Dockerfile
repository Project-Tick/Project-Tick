FROM gcr.io/oss-fuzz-base/base-builder@sha256:def08ca2af228f6d14df4110e0513a86ca45530847e2d1b3884ab5a385f352f0

RUN apt-get update && apt-get install -y cmake make

COPY . $SRC/project-tick
WORKDIR $SRC/project-tick

COPY .clusterfuzzlite/build.sh $SRC/build.sh
