# syntax=docker/dockerfile:1.23
FROM debian:stable-slim

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     apt-get update; apt-get install -y --no-install-recommends ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     rm -rf /var/lib/apt/lists/*;   fi;   apt-get update && apt-get install -y --no-install-recommends qt6-base-dev && rm -rf /var/lib/apt/lists/*;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ]; then     true;   else     echo "Qt6 toolchain not found" >&2;     exit 1;   fi

CMD ["/bin/sh"]
