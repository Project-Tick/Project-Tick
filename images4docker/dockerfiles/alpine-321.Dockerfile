# syntax=docker/dockerfile:1.7@sha256:a57df69d0ea827fb7266491f2813635de6f17269be881f696fbfdf2d83dda33e
FROM alpine:3.21@sha256:48b0309ca019d89d40f670aa1bc06e426dc0931948452e8491e3d65087abc07d

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     apk add --no-cache ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     true;   fi;   apk add --no-cache qt6-qtbase-dev;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
