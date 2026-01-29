#!/usr/bin/bash

set -eo pipefail

# Use grouped output messages
infobegin() {
	echo "::group::${1}"
}
infoend() {
	echo "::endgroup::"
}

# Required packages on Archlinux
requires=(
	ccache # Use ccache to speed up build
	clang  # Build with clang on Archlinux
	meson  # Used for meson build
)

# https://gitlab.archlinux.org/archlinux/packaging/packages/mate-terminal
requires+=(
	autoconf-archive
	gcc
	gettext
	git
	glib2-devel
	itstool
	libsm
	make
	mate-common
	mate-desktop
	perl
	python
	vte3
	which
	yelp-tools
)

infobegin "Update system"
pacman --noconfirm -Syu
infoend

infobegin "Install dependency packages"
pacman --noconfirm -S ${requires[@]}
infoend
