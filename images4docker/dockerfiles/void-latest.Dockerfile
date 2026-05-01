# syntax=docker/dockerfile:1.7@sha256:a57df69d0ea827fb7266491f2813635de6f17269be881f696fbfdf2d83dda33e
FROM voidlinux/voidlinux:latest@sha256:26ba972f0c06beadcec4796ec3037e0bec32af4d255edb68a528bd98304c74f4

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     xbps-install -Sy ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     xbps-remove -O || true;   fi;   xbps-install -S && xbps-install -y qt6-base-devel || echo "Qt6 not available on this base image (skipped)" >&2;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ] || [ -x /usr/libexec/qt6/qmake ] || [ -x /usr/libexec/qt6/qtpaths ]; then     true;   else     echo "Qt6 not available on this base image (skipped)" >&2;   fi

CMD ["/bin/sh"]
