/*
 *  caja-open-terminal.c
 * 
 *  Copyright (C) 2004, 2005 Free Software Foundation, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Christian Neumair <chris@mate-de.org>
 * 
 */

#ifdef HAVE_CONFIG_H
 #include <config.h> /* for GETTEXT_PACKAGE */
#endif

#include "caja-open-terminal.h"
#include "eel-mate-extensions.h"

#include <libcaja-extension/caja-menu-provider.h>

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <mateconf/mateconf.h>
#include <mateconf/mateconf-client.h>
#include <libmate/mate-desktop-item.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h> /* for atoi */
#include <string.h> /* for strcmp */
#include <unistd.h> /* for chdir */
#include <sys/stat.h>

static void caja_open_terminal_instance_init (CajaOpenTerminal      *cvs);
static void caja_open_terminal_class_init    (CajaOpenTerminalClass *class);

static GType terminal_type = 0;

typedef enum {
	/* local files. Always open "conventionally", i.e. cd and spawn. */
	FILE_INFO_LOCAL,
	FILE_INFO_DESKTOP,
	/* SFTP: Shell terminals are opened "remote" (i.e. with ssh client),
	 * commands are executed like *_OTHER */
	FILE_INFO_SFTP,
	/* OTHER: Terminals and commands are opened by mapping the URI back
	 * to ~/.gvfs, i.e. to the GVFS FUSE bridge
	 */
	FILE_INFO_OTHER
} TerminalFileInfo;

static TerminalFileInfo
get_terminal_file_info (const char *uri)
{
	TerminalFileInfo ret;
	char *uri_scheme;

	uri_scheme = g_uri_parse_scheme (uri);

	if (uri_scheme == NULL) {
		ret = FILE_INFO_OTHER;
	} else if (strcmp (uri_scheme, "file") == 0) {
		ret = FILE_INFO_LOCAL;
	} else if (strcmp (uri_scheme, "x-caja-desktop") == 0) {
		ret = FILE_INFO_DESKTOP;
	} else if (strcmp (uri_scheme, "sftp") == 0 ||
		   strcmp (uri_scheme, "ssh") == 0) {
		ret = FILE_INFO_SFTP;
	} else {
		ret = FILE_INFO_OTHER;
	}

	g_free (uri_scheme);

	return ret;
}

static MateConfClient *mateconf_client = NULL;

static inline gboolean
desktop_opens_home_dir (void)
{
	return mateconf_client_get_bool (mateconf_client,
				      "/apps/caja-open-terminal/desktop_opens_home_dir",
				      NULL);
}

static inline gboolean
display_mc_item (void)
{
	return mateconf_client_get_bool (mateconf_client,
				      "/apps/caja-open-terminal/display_mc_item",
				      NULL);
}

static inline gboolean
desktop_is_home_dir ()
{
	return mateconf_client_get_bool (mateconf_client,
				      "/apps/caja/preferences/desktop_is_home_dir",
				      NULL);
}

/* a very simple URI parsing routine from Launchpad #333462, until GLib supports URI parsing (MATE #489862) */
#define SFTP_PREFIX "sftp://"
static void
parse_sftp_uri (GFile *file,
		char **user,
		char **host,
		unsigned int *port,
		char **path)
{
	char *tmp, *save;
	char *uri;

	uri = g_file_get_uri (file);
	g_assert (uri != NULL);
	save = uri;

	*path = NULL;
	*user = NULL;
	*host = NULL;
	*port = 0;

	/* skip intial 'sftp:// prefix */
	g_assert (!strncmp (uri, SFTP_PREFIX, strlen (SFTP_PREFIX)));
	uri += strlen (SFTP_PREFIX);

	/* cut out the path */
	tmp = strchr (uri, '/');
	if (tmp != NULL) {
		*path = g_uri_unescape_string (tmp, "/");
		*tmp = '\0';
	}

	/* read the username - it ends with @ */
	tmp = strchr (uri, '@');
	if (tmp != NULL) {
		*tmp++ = '\0';

		*user = strdup (uri);
		if (strchr (*user, ':') != NULL) {
			/* chop the password */
			*(strchr (*user, ':')) = '\0'; 
		}

		uri = tmp;
	}

	/* now read the port, starts with : */
	tmp = strchr (uri, ':');
	if (tmp != NULL) {
		*tmp++ = '\0';
		*port = atoi (tmp);  /*FIXME: getservbyname*/
	}

	/* what is left is the host */
	*host = strdup (uri);
	g_free (save);
}

static char *
get_remote_ssh_command (const char *uri,
			const char *command_to_run)
{
	GFile *file;

	char *host_name, *path, *user_name;
	char *command, *user_host, *unescaped_path;
	char *quoted_path;
	char *remote_command;
	char *quoted_remote_command;
	char *port_str;
	guint host_port;

	g_assert (uri != NULL);

	file = g_file_new_for_uri (uri);
	parse_sftp_uri (file, &user_name, &host_name, &host_port, &path);
	g_object_unref (file);

	/* FIXME to we have to consider the remote file encoding? */
	unescaped_path = g_uri_unescape_string (path, NULL);
	quoted_path = g_shell_quote (unescaped_path);

	port_str = NULL;
	if (host_port != 0) {
		port_str = g_strdup_printf (" -p %d", host_port);
	} else {
		port_str = g_strdup ("");
	}

	if (user_name != NULL) {
		user_host = g_strdup_printf ("%s@%s", user_name, host_name);
	} else {
		user_host = g_strdup (host_name);
	}

	if (command_to_run != NULL) {
		remote_command = g_strdup_printf ("cd %s && exec %s", quoted_path, command_to_run);
	} else {
		remote_command = g_strdup_printf ("cd %s && exec $SHELL -l", quoted_path);
	}

	quoted_remote_command = g_shell_quote (remote_command);

	command = g_strdup_printf ("ssh %s%s -t %s", user_host, port_str, quoted_remote_command);

	g_free (user_name);
	g_free (user_host);
	g_free (host_name);
	g_free (port_str);

	g_free (path);
	g_free (unescaped_path);
	g_free (quoted_path);

	g_free (remote_command);
	g_free (quoted_remote_command);

	return command;
}

static inline char *
get_gvfs_path_for_uri (const char *uri)
{
	GFile *file;
	char *path;

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);
	g_object_unref (file);

	return path;
}

static char *
get_terminal_command_for_file_info (cajaFileInfo *file_info,
				    const char *command_to_run,
				    gboolean remote_terminal)
{
	char *uri, *path, *quoted_path;
	char *command;

	uri = caja_file_info_get_activation_uri (file_info);

	path = NULL;
	command = NULL;

	switch (get_terminal_file_info (uri)) {
		case FILE_INFO_LOCAL:
			if (uri != NULL) {
				path = g_filename_from_uri (uri, NULL, NULL);
			}
			break;

		case FILE_INFO_DESKTOP:
			if (desktop_is_home_dir () || desktop_opens_home_dir ()) {
				path = g_strdup (g_get_home_dir ());
			} else {
				path = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP));
			}
			break;

		case FILE_INFO_SFTP:
			if (remote_terminal && uri != NULL) {
				command = get_remote_ssh_command (uri, command_to_run);
				break;
			}

			/* fall through */
		case FILE_INFO_OTHER:
			if (uri != NULL) {
				/* map back remote URI to local path */
				path = get_gvfs_path_for_uri (uri);
			}
			break;

		default:
			g_assert_not_reached ();
	}

	if (command == NULL && path != NULL) {
		quoted_path = g_shell_quote (path);

		if (command_to_run != NULL) {
			command = g_strdup_printf ("cd %s && exec %s", quoted_path, command_to_run);
		} else {
			command = g_strdup_printf ("cd %s && exec $SHELL -l", quoted_path);
		}

		g_free (quoted_path);
	}

	g_free (path);
	g_free (uri);

	return command;
}


static void
open_terminal (cajaMenuItem *item,
	       cajaFileInfo *file_info)
{
	char *terminal_command, *command_to_run;
	GdkScreen *screen;
	gboolean remote_terminal;

	screen = g_object_get_data (G_OBJECT (item), "CajaOpenTerminal::screen");
	command_to_run = g_object_get_data (G_OBJECT (item), "CajaOpenTerminal::command-to-run");
	remote_terminal = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (item), "CajaOpenTerminal::remote-terminal"));

	terminal_command = get_terminal_command_for_file_info (file_info, command_to_run, remote_terminal);
	if (terminal_command != NULL) {
		_not_eel_mate_open_terminal_on_screen (terminal_command, screen);
	}
	g_free (terminal_command);
}

static void
open_terminal_callback (cajaMenuItem *item,
			cajaFileInfo *file_info)
{
	open_terminal (item, file_info);
}

static cajaMenuItem *
open_terminal_menu_item_new (cajaFileInfo *file_info,
			     TerminalFileInfo  terminal_file_info,
			     GdkScreen        *screen,
			     const char       *command_to_run,
			     gboolean          remote_terminal,
			     gboolean          is_file_item)
{
	cajaMenuItem *ret;
	char *action_name;
	const char *name;
	const char *tooltip;

	if (command_to_run == NULL) {
		switch (terminal_file_info) {
			case FILE_INFO_SFTP:
				if (remote_terminal) {
					name = _("Open in _Remote Terminal");
				} else {
					name = _("Open in _Local Terminal");
				}

				if (is_file_item) {
					tooltip = _("Open the currently selected folder in a terminal");
				} else {
					tooltip = _("Open the currently open folder in a terminal");
				}
				break;

			case FILE_INFO_LOCAL:
			case FILE_INFO_OTHER:
				name = _("Open in T_erminal");

				if (is_file_item) {
					tooltip = _("Open the currently selected folder in a terminal");
				} else {
					tooltip = _("Open the currently open folder in a terminal");
				}
				break;

			case FILE_INFO_DESKTOP:
				if (desktop_opens_home_dir ()) {
					name = _("Open T_erminal");
					tooltip = _("Open a terminal");
				} else {
					name = _("Open in T_erminal");
					tooltip = _("Open the currently open folder in a terminal");
				}
				break;

			default:
				g_assert_not_reached ();
		}
	} else if (!strcmp (command_to_run, "mc")) {
		switch (terminal_file_info) {
			case FILE_INFO_LOCAL:
			case FILE_INFO_SFTP:
			case FILE_INFO_OTHER:
				name = _("Open in _Midnight Commander");
				if (is_file_item) {
					tooltip = _("Open the currently selected folder in the terminal file manager Midnight Commander");
				} else {
					tooltip = _("Open the currently open folder in the terminal file manager Midnight Commander");
				}
				break;

			case FILE_INFO_DESKTOP:
				if (desktop_opens_home_dir ()) {
					name = _("Open _Midnight Commander");
					tooltip = _("Open the terminal file manager Midnight Commander");
				} else {
					name = _("Open in _Midnight Commander");
					tooltip = _("Open the currently open folder in the terminal file manager Midnight Commander");
				}
				break;

			default:
				g_assert_not_reached ();
		}
	} else {
		g_assert_not_reached ();
	}

	if (command_to_run != NULL) {
		action_name = g_strdup_printf (remote_terminal ?
					       "CajaOpenTerminal::open_remote_terminal_%s" :
					       "CajaOpenTerminal::open_terminal_%s",
					       command_to_run);
	} else {
		action_name = g_strdup (remote_terminal ? 
					"CajaOpenTerminal::open_remote_terminal" :
					"CajaOpenTerminal::open_terminal");
	}
	ret = caja_menu_item_new (action_name, name, tooltip, "utilities-terminal");
	g_free (action_name);

	g_object_set_data (G_OBJECT (ret),
			   "CajaOpenTerminal::screen",
			   screen);
	g_object_set_data_full (G_OBJECT (ret), "CajaOpenTerminal::command-to-run",
				g_strdup (command_to_run),
				(GDestroyNotify) g_free);
	g_object_set_data (G_OBJECT (ret), "CajaOpenTerminal::remote-terminal",
			   GUINT_TO_POINTER (remote_terminal));


	g_object_set_data_full (G_OBJECT (ret), "file-info",
				g_object_ref (file_info),
				(GDestroyNotify) g_object_unref);


	g_signal_connect (ret, "activate",
			  G_CALLBACK (open_terminal_callback),
			  file_info);


	return ret;
}

static gboolean
terminal_locked_down (void)
{
	return mateconf_client_get_bool (mateconf_client,
                                      "/desktop/mate/lockdown/disable_command_line",
                                      NULL);
}

/* used to determine for remote URIs whether GVFS is capable of mapping them to ~/.gvfs */
static gboolean
uri_has_local_path (const char *uri)
{
	GFile *file;
	char *path;
	gboolean ret;

	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);

	ret = (path != NULL);

	g_free (path);
	g_object_unref (file);

	return ret;
}

static GList *
caja_open_terminal_get_background_items (cajaMenuProvider *provider,
					     GtkWidget		  *window,
					     cajaFileInfo	  *file_info)
{
	gchar *uri;
	GList *items;
	cajaMenuItem *item;
	TerminalFileInfo  terminal_file_info;

        if (terminal_locked_down ()) {
            return NULL;
        }

	items = NULL;

	uri = caja_file_info_get_activation_uri (file_info);
	terminal_file_info = get_terminal_file_info (uri);

	if (terminal_file_info == FILE_INFO_SFTP ||
	    terminal_file_info == FILE_INFO_DESKTOP ||
	    uri_has_local_path (uri)) {
		/* local locations or SSH */
		item = open_terminal_menu_item_new (file_info, terminal_file_info, gtk_widget_get_screen (window),
						    NULL, terminal_file_info == FILE_INFO_SFTP, FALSE);
		items = g_list_append (items, item);
	}

	if ((terminal_file_info == FILE_INFO_SFTP ||
	     terminal_file_info == FILE_INFO_OTHER) &&
	    uri_has_local_path (uri)) {
		/* remote locations that offer local back-mapping */
		item = open_terminal_menu_item_new (file_info, terminal_file_info, gtk_widget_get_screen (window),
						    NULL, FALSE, FALSE);
		items = g_list_append (items, item);
	}

	if (display_mc_item () &&
	    g_find_program_in_path ("mc") &&
	    ((terminal_file_info == FILE_INFO_DESKTOP &&
	      (desktop_is_home_dir () || desktop_opens_home_dir ())) ||
	     uri_has_local_path (uri))) {
		item = open_terminal_menu_item_new (file_info, terminal_file_info, gtk_widget_get_screen (window), "mc", FALSE, FALSE);
		items = g_list_append (items, item);
	}

	g_free (uri);

	return items;
}

GList *
caja_open_terminal_get_file_items (cajaMenuProvider *provider,
				       GtkWidget            *window,
				       GList                *files)
{
	gchar *uri;
	GList *items;
	cajaMenuItem *item;
	TerminalFileInfo  terminal_file_info;

        if (terminal_locked_down ()) {
            return NULL;
        }

	if (g_list_length (files) != 1 ||
	    (!caja_file_info_is_directory (files->data) &&
	     caja_file_info_get_file_type (files->data) != G_FILE_TYPE_SHORTCUT &&
	     caja_file_info_get_file_type (files->data) != G_FILE_TYPE_MOUNTABLE)) {
		return NULL;
	}

	items = NULL;

	uri = caja_file_info_get_activation_uri (files->data);
	terminal_file_info = get_terminal_file_info (uri);

	switch (terminal_file_info) {
		case FILE_INFO_LOCAL:
		case FILE_INFO_SFTP:
		case FILE_INFO_OTHER:
			if (terminal_file_info == FILE_INFO_SFTP || uri_has_local_path (uri)) {
				item = open_terminal_menu_item_new (files->data, terminal_file_info, gtk_widget_get_screen (window),
								    NULL, terminal_file_info == FILE_INFO_SFTP, TRUE);
				items = g_list_append (items, item);
			}

			if (terminal_file_info == FILE_INFO_SFTP &&
			    uri_has_local_path (uri)) {
				item = open_terminal_menu_item_new (files->data, terminal_file_info, gtk_widget_get_screen (window), NULL, FALSE, TRUE);
				items = g_list_append (items, item);
			}

			if (display_mc_item () &&
			    g_find_program_in_path ("mc") &&
			     uri_has_local_path (uri)) {
				item = open_terminal_menu_item_new (files->data, terminal_file_info, gtk_widget_get_screen (window), "mc", TRUE, FALSE);
				items = g_list_append (items, item);
			}
			break;

		case FILE_INFO_DESKTOP:
			break;

		default:
			g_assert_not_reached ();
	}

	g_free (uri);

	return items;
}

static void
caja_open_terminal_menu_provider_iface_init (cajaMenuProviderIface *iface)
{
	iface->get_background_items = caja_open_terminal_get_background_items;
	iface->get_file_items = caja_open_terminal_get_file_items;
}

static void 
caja_open_terminal_instance_init (CajaOpenTerminal *cvs)
{
}

static void
caja_open_terminal_class_init (CajaOpenTerminalClass *class)
{
	g_assert (mateconf_client == NULL);
	mateconf_client = mateconf_client_get_default ();
}

static void
caja_open_terminal_class_finalize (CajaOpenTerminalClass *class)
{
	g_assert (mateconf_client != NULL);
	g_object_unref (mateconf_client);
	mateconf_client = NULL;
}

GType
caja_open_terminal_get_type (void) 
{
	return terminal_type;
}

void
caja_open_terminal_register_type (GTypeModule *module)
{
	static const GTypeInfo info = {
		sizeof (CajaOpenTerminalClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) caja_open_terminal_class_init,
		(GClassFinalizeFunc) caja_open_terminal_class_finalize,
		NULL,
		sizeof (CajaOpenTerminal),
		0,
		(GInstanceInitFunc) caja_open_terminal_instance_init,
	};

	static const GInterfaceInfo menu_provider_iface_info = {
		(GInterfaceInitFunc) caja_open_terminal_menu_provider_iface_init,
		NULL,
		NULL
	};

	terminal_type = g_type_module_register_type (module,
						     G_TYPE_OBJECT,
						     "CajaOpenTerminal",
						     &info, 0);

	g_type_module_add_interface (module,
				     terminal_type,
				     caja_TYPE_MENU_PROVIDER,
				     &menu_provider_iface_info);
}
