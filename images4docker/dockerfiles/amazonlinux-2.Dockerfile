# syntax=docker/dockerfile:1.23@sha256:2780b5c3bab67f1f76c781860de469442999ed1a0d7992a5efdf2cffc0e3d769
FROM amazonlinux:2@sha256:4b3bb9ce33d248600f267f2acc965325587b3bcfaae72fe0bf9cff85fbbb17f6

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     yum install -y ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     yum clean all || true;   fi;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 not available on this base image (skipped)" >&2;   fi

CMD ["/bin/sh"]
