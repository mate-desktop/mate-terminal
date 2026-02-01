/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008, 2010 Christian Persch
 * Copyright (C) 2012-2021 MATE Developers
 *
 * Mate-terminal is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mate-terminal is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include <errno.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#ifdef HAVE_SMCLIENT
#include "eggsmclient.h"
#endif /* HAVE_SMCLIENT */

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"

int
main (int argc, char **argv)
{
	TerminalApp *app;
	int ret;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, TERM_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	_terminal_debug_init ();

	g_set_application_name (_("Terminal"));

	/* Create the GtkApplication */
	app = g_object_new (TERMINAL_TYPE_APP,
	                    "application-id", TERMINAL_APPLICATION_ID,
	                    "flags", G_APPLICATION_HANDLES_COMMAND_LINE,
	                    NULL);

	/* Run the application */
	ret = g_application_run (G_APPLICATION (app), argc, argv);

	g_object_unref (app);

	return ret;
}
