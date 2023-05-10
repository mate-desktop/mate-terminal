m4_define(mt_major_version, 1)
m4_define(mt_minor_version, 26)
m4_define(mt_micro_version, 1)
m4_define(mt_version, mt_major_version.mt_minor_version.mt_micro_version)

m4_define(mt_api_version, 0)

AC_INIT([mate-terminal],[mt_version],
        [https://github.com/mate-desktop/mate-terminal/issues],
        [mate-terminal], [https://mate-desktop.org])
AC_CONFIG_SRCDIR([src/terminal.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 foreign no-dist-gzip dist-xz check-news])

AC_SUBST(TERMINAL_MAJOR_VERSION, mt_major_version)
AC_SUBST(TERMINAL_MINOR_VERSION, mt_minor_version)
AC_SUBST(TERMINAL_MICRO_VERSION, mt_micro_version)
AC_SUBST(TERMINAL_API_VERSION, mt_api_version)

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

if test -z "$enable_maintainer_mode"; then
  enable_maintainer_mode=yes
fi
AM_MAINTAINER_MODE([enable])

MATE_COMMON_INIT
MATE_DEBUG_CHECK([no])
MATE_COMPILE_WARNINGS
#MATE_MAINTAINER_MODE_DEFINES

AC_C_BIGENDIAN

PKG_PROG_PKG_CONFIG
AC_PROG_CC
AC_HEADER_STDC
AM_PROG_LIBTOOL

GLIB_REQUIRED=2.50.0
GIO_REQUIRED=2.50.0
GTK_REQUIRED=3.22.0
VTE_REQUIRED=0.48
DCONF_REQUIRED=0.13.4

PKG_CHECK_MODULES([TERM],
  [vte-2.91 >= $VTE_REQUIRED
   glib-2.0 >= $GLIB_REQUIRED
   gthread-2.0
   gio-2.0 >= $GIO_REQUIRED
   gtk+-3.0 >= $GTK_REQUIRED
   dconf >= $DCONF_REQUIRED
   x11])

# ********
# smclient
# ********

# $enable_xsmp will be set to "yes" or "no"
AC_ARG_ENABLE(smclient,
              [AC_HELP_STRING([--enable-smclient],
                              [Explicitly enable or disable smclient])],
              [enable_smclient=$enableval],
              [enable_smclient=yes])

have_sm=no
if test "x$enable_smclient" != "xno"; then
  PKG_CHECK_MODULES(LIBEGG, sm >= 1.0.0 ice >= 1.0.0 gtk+-3.0, have_sm=yes, [
        if test "x$enable_smclient" = "xyes"; then
          AC_MSG_ERROR([smclient enabled but SM library not found])
        fi
    ])
fi

AM_CONDITIONAL(ENABLE_SMCLIENT, [test "x$have_sm" = "xyes"])

if test "x$have_sm" = "xyes"; then
  AC_DEFINE(HAVE_SMCLIENT, 1, [Have the SM development library])
fi

AC_SUBST([LIBEGG_CFLAGS])
AC_SUBST([LIBEGG_LIBS])

# *****
# GSettings
# *****

GLIB_GSETTINGS

GLIB_GENMARSHAL="$($PKG_CONFIG --variable=glib_genmarshal glib-2.0)"
AC_SUBST([GLIB_GENMARSHAL])
GLIB_MKENUMS="$($PKG_CONFIG --variable=glib_mkenums glib-2.0)"
AC_SUBST([GLIB_MKENUMS])

# **********************
# GLIB_COMPILE_RESOURCES
# **********************
AC_ARG_VAR([GLIB_COMPILE_RESOURCES],[glib-compile-resources bin])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES],[glib-compile-resources],[])
if test -z "$GLIB_COMPILE_RESOURCES"; then
  AC_MSG_ERROR([glib-compile-resources not found])
fi

# *******
# XMLLINT
# *******
AC_ARG_VAR([XMLLINT],[xmllint bin])
AC_PATH_PROG([XMLLINT],[xmllint],[])
if test -z "$XMLLINT"; then
  AC_MSG_ERROR([xmllint not found])
fi

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

# ****************************
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT_REQUIRE_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE=mate-terminal
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])

AC_CONFIG_FILES([
Makefile
mate-terminal.appdata.xml.in
mate-terminal.desktop.in
src/Makefile
src/mate-submodules/Makefile
src/mate-submodules/libegg/Makefile
src/org.mate.terminal.gschema.xml
src/terminal-version.h
help/Makefile
po/Makefile.in
])

AM_COND_IF([ENABLE_SKEY],
           [AC_CONFIG_FILES([src/skey/Makefile])])

AC_OUTPUT

# ****************************

echo "
Configure summary:

	${PACKAGE_STRING}
	`echo $PACKAGE_STRING | sed "s/./=/g"`

	prefix:                 ${prefix}
	source code location:   ${srcdir}
	compiler:               ${CC}
	cflags:                 ${CFLAGS}
	warning flags:          ${WARN_CFLAGS}
	linker flags:           ${LDFLAGS}

	smclient support:       ${have_sm}
	s/key support:          ${enable_skey}
"
