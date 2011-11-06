INCLUDES =						\
	-DG_LOG_DOMAIN=\"Caja-Open-Terminal\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DMATELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(WARN_CFLAGS)                                  \
	-Werror                                         \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(CAJA_CFLAGS)				\
	$(MATECONF_CFLAGS)					\
	$(MATEDESKTOP_CFLAGS)

caja_extensiondir=$(CAJA_EXTENSION_DIR)

caja_extension_LTLIBRARIES=libcaja-open-terminal.la

libcaja_open_terminal_la_SOURCES = \
	eel-mate-extensions.c       \
	eel-mate-extensions.h       \
	caja-open-terminal.h       \
	caja-open-terminal.c       \
	caja-open-terminal.h       \
	open-terminal.c

libcaja_open_terminal_la_LDFLAGS = -module -avoid-version
libcaja_open_terminal_la_LIBADD  = $(CAJA_LIBS) $(MATECONF_LIBS) $(MATEDESKTOP_LIBS)
