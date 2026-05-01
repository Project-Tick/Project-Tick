FROM gcr.io/oss-fuzz-base/base-builder@sha256:56a1d6eaf314de9f38e10e7d26078dcb2a66e6d77612aaeb06e8cc208c72d089

RUN apt-get update && apt-get install -y cmake make

COPY . $SRC/project-tick
WORKDIR $SRC/project-tick

COPY .clusterfuzzlite/build.sh $SRC/build.sh
