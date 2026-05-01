FROM ghcr.io/flathub/flatpak-builder-lint:latest@sha256:55ab266774df16f13548c19b2b990c2041425bb90389267ef324856ff21d3fce

ENTRYPOINT []
ENV TMPDIR=/tmp

RUN flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
RUN flatpak remote-add --if-not-exists flathub-beta https://flathub.org/beta-repo/flathub-beta.flatpakrepo
RUN flatpak remote-add --if-not-exists gnome-nightly https://nightly.gnome.org/gnome-nightly.flatpakrepo
