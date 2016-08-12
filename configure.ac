m4_define([gt_version_major],[1])
m4_define([gt_version_minor],[14])
m4_define([gt_version_micro],[1])
m4_define([gt_version_extra],[])
m4_define([gt_version],[gt_version_major().gt_version_minor().gt_version_micro()gt_version_extra])

m4_define([gt_api_version],[0])

AC_INIT([MATE Terminal],[gt_version],[http://www.mate-desktop.org],[mate-terminal])
AC_CONFIG_SRCDIR([src/terminal.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-xz check-news])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

GETTEXT_PACKAGE=mate-terminal
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

AC_C_BIGENDIAN

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL
IT_PROG_INTLTOOL([0.50.1])

MATE_COMMON_INIT
MATE_DEBUG_CHECK
MATE_COMPILE_WARNINGS([maximum])
#MATE_MAINTAINER_MODE_DEFINES

# GLib min/max required versions
AC_DEFINE([GLIB_VERSION_MAX_ALLOWED], [GLIB_VERSION_2_36],
	[Warn on use of APIs added after GLib 2.36])
AC_DEFINE([GLIB_VERSION_MIN_REQUIRED], [GLIB_VERSION_2_36],
	[Warn on use of APIs deprecated before GLib 2.36])

AM_GLIB_GNU_GETTEXT

GLIB_REQUIRED=2.36.0
GIO_REQUIRED=2.25.12
MATE_DESKTOP_REQUIRED=1.9.0

AC_MSG_CHECKING([which gtk+ version to compile against])
AC_ARG_WITH([gtk],
  [AS_HELP_STRING([--with-gtk=2.0|3.0],[which gtk+ version to compile against (default: 2.0)])],
  [case "$with_gtk" in
     2.0|3.0) ;;
     *) AC_MSG_ERROR([invalid gtk version specified]) ;;
   esac],
  [with_gtk=2.0])
AC_MSG_RESULT([$with_gtk])

case "$with_gtk" in
  2.0) GTK_API_VERSION=2.0
       GTK_REQUIRED=2.24.0
       GDK_TARGETS="$($PKG_CONFIG --variable target gdk-$GTK_API_VERSION)"
       VTE_REQUIRED=0.28
       VTE_PC_VERSION=
       ;;
  3.0) GTK_API_VERSION=3.0
       GTK_REQUIRED=3.0.0
       GDK_TARGETS="$($PKG_CONFIG --variable targets gdk-$GTK_API_VERSION)"
       VTE_REQUIRED=0.38
       VTE_PC_VERSION=-2.91
       ;;
esac

PKG_CHECK_MODULES([TERM],
  [vte$VTE_PC_VERSION >= $VTE_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gthread-2.0
   gio-2.0 >= $GIO_REQUIRED
   gtk+-$GTK_API_VERSION >= $GTK_REQUIRED
   mate-desktop-2.0 >= $MATE_DESKTOP_REQUIRED
   x11])

# ********
# smclient
# ********

PKG_CHECK_MODULES(SMCLIENT, sm >= 1.0.0)
AC_SUBST([SMCLIENT_CFLAGS])
AC_SUBST([SMCLIENT_LIBS])

# *****
# GSettings
# *****

GLIB_GSETTINGS

GLIB_GENMARSHAL="$($PKG_CONFIG --variable=glib_genmarshal glib-2.0)"
AC_SUBST([GLIB_GENMARSHAL])
GLIB_MKENUMS="$($PKG_CONFIG --variable=glib_mkenums glib-2.0)"
AC_SUBST([GLIB_MKENUMS])

# *****
# S/Key
# *****

AC_MSG_CHECKING([whether to enable s/key support])
AC_ARG_ENABLE([skey],
  [AS_HELP_STRING([--disable-skey],[disable s/key support (default: enabled)])],
  [],[enable_skey=yes])
AC_MSG_RESULT([$enable_skey])

if test "$enable_skey" = "yes"; then
  AC_DEFINE([ENABLE_SKEY],[1],[Define if s/key support is enabled])
fi

AM_CONDITIONAL([ENABLE_SKEY],[test "$enable_skey" = "yes"])

# *************
# Documentation
# *************

YELP_HELP_INIT

# ****************************

AC_SUBST([TERMINAL_MAJOR_VERSION],[gt_version_major])
AC_SUBST([TERMINAL_MINOR_VERSION],[gt_version_minor])
AC_SUBST([TERMINAL_MICRO_VERSION],[gt_version_micro])
AC_SUBST([TERMINAL_API_VERSION],[gt_api_version])

# ****************************

AC_CONFIG_FILES([
Makefile
mate-terminal.desktop.in
src/Makefile
src/org.mate.terminal.gschema.xml
src/terminal-version.h
src/skey/Makefile
help/Makefile
po/Makefile.in
])
AC_OUTPUT

# ****************************

echo "
	mate-terminal-$VERSION:

	prefix:                 ${prefix}
	source code location:   ${srcdir}
	compiler:               ${CC}

	s/key support:          ${enable_skey}
"
