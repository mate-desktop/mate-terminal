libcaja_open_terminal_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"Caja-Open-Terminal\"	\
	-DDATADIR=\"$(datadir)\"			\
	-DMATELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(top_srcdir)					\
	-I$(top_builddir)				\
	$(WARN_CFLAGS)                                  \
	-Werror                                         \
	$(DISABLE_DEPRECATED_CFLAGS)			\
	$(AM_CFLAGS)				\
	$(TERM_CFLAGS)				\
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
libcaja_open_terminal_la_LIBADD  = $(AM_LIBS) $(TERM_LIBS) $(MATEDESKTOP_LIBS)
