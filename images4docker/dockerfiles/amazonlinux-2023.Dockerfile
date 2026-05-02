# syntax=docker/dockerfile:1.23@sha256:2780b5c3bab67f1f76c781860de469442999ed1a0d7992a5efdf2cffc0e3d769
FROM amazonlinux:2023@sha256:ceeab7e010ed03ea155cfbbfd7140672eba5a49e1110b8b4ed35342312c3f21a

ARG PACKAGES=
ARG CUSTOM_INSTALL=
ARG UPDATE_CMD=
ARG CLEAN_CMD=

SHELL ["/bin/sh", "-lc"]

RUN set -eux;   if [ -n "${UPDATE_CMD}" ]; then     sh -lc "${UPDATE_CMD}";   fi;   if [ -n "${CUSTOM_INSTALL}" ]; then     sh -lc "${CUSTOM_INSTALL}";   elif [ -n "${PACKAGES}" ]; then     dnf install -y ${PACKAGES};   fi;   if [ -n "${CLEAN_CMD}" ]; then     sh -lc "${CLEAN_CMD}";   else     dnf clean all || true;   fi;   dnf install -y qt6-qtbase-devel && dnf clean all || echo "Qt6 not available on this base image (skipped)" >&2;   export PATH="$PATH:/usr/lib/qt6/bin:/usr/lib64/qt6/bin:/usr/libexec/qt6:/opt/qt6/bin:/root/.nix-profile/bin";   if command -v qmake6 >/dev/null 2>&1 || command -v qmake-qt6 >/dev/null 2>&1 || command -v qtpaths6 >/dev/null 2>&1 || [ -x /usr/lib/qt6/bin/qmake ] || [ -x /usr/lib64/qt6/bin/qmake ] || [ -x /usr/lib/qt6/bin/qtpaths ] || [ -x /usr/lib64/qt6/bin/qtpaths ] || [ -x /usr/libexec/qt6/qmake ] || [ -x /usr/libexec/qt6/qtpaths ]; then     true;   else     echo "Qt6 not available on this base image (skipped)" >&2;   fi

CMD ["/bin/sh"]
