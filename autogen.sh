#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME=mate-terminal

(test -f $srcdir/configure.ac \
  && test -f $srcdir/autogen.sh \
  && test -d $srcdir/src \
  && test -f $srcdir/src/terminal.c) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which mate-autogen.sh || {
    echo "You need to install mate-common from the MATE CVS"
    exit 1
}

REQUIRED_AUTOMAKE_VERSION=1.9 MATE_DATADIR="$mate_datadir" . mate-autogen.sh
