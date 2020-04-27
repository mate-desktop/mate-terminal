/*
 * Copyright © 2001, 2002 Havoc Pennington
 * Copyright © 2002 Red Hat, Inc.
 * Copyright © 2002 Sun Microsystems
 * Copyright © 2003 Mariano Suarez-Alvarez
 * Copyright © 2008 Christian Persch
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

#include <glib.h>
#include <dconf.h>

#include "terminal-intl.h"

#include "terminal-debug.h"
#include "terminal-app.h"
#include "terminal-accels.h"
#include "terminal-screen.h"
#include "terminal-screen-container.h"
#include "terminal-window.h"
#include "terminal-util.h"
#include "profile-editor.h"
#include "terminal-encoding.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

#ifdef HAVE_SMCLIENT
#include "eggsmclient.h"
#include "eggdesktopfile.h"
#endif /* HAVE_SMCLIENT */

#define FALLBACK_PROFILE_ID "default"

/* Settings storage works as follows:
 *   /apps/mate-terminal/global/
 *   /apps/mate-terminal/profiles/Foo/
 *
 * It's somewhat tricky to manage the profiles/ dir since we need to track
 * the list of profiles, but GSettings doesn't have a concept of notifying that
 * a directory has appeared or disappeared.
 *
 * Session state is stored entirely in the RestartCommand command line.
 *
 * The number one rule: all stored information is EITHER per-session,
 * per-profile, or set from a command line option. THERE CAN BE NO
 * OVERLAP. The UI and implementation totally break if you overlap
 * these categories. See mate-terminal 1.x for why.
 *
 * Don't use this code as an example of how to use GSettings - it's hugely
 * overcomplicated due to the profiles stuff. Most apps should not
 * have to do scary things of this nature, and should not have
 * a profiles feature.
 *
 */

GSettings *settings_global;

struct _TerminalAppClass
{
	GObjectClass parent_class;

	void (* quit) (TerminalApp *app);
	void (* profile_list_changed) (TerminalApp *app);
	void (* encoding_list_changed) (TerminalApp *app);
};

struct _TerminalApp
{
	GObject parent_instance;

	GList *windows;
	GtkWidget *new_profile_dialog;
	GtkWidget *manage_profiles_dialog;
	GtkWidget *manage_profiles_list;
	GtkWidget *manage_profiles_new_button;
	GtkWidget *manage_profiles_edit_button;
	GtkWidget *manage_profiles_delete_button;
	GtkWidget *manage_profiles_default_menu;

	GSettings *settings_font;

	GHashTable *profiles;
	char* default_profile_id;
	TerminalProfile *default_profile;
	gboolean default_profile_locked;

	GHashTable *encodings;
	gboolean encodings_locked;

	PangoFontDescription *system_font_desc;
	gboolean enable_mnemonics;
	gboolean enable_menu_accels;
};

enum
{
    PROP_0,
    PROP_DEFAULT_PROFILE,
    PROP_ENABLE_MENU_BAR_ACCEL,
    PROP_ENABLE_MNEMONICS,
    PROP_SYSTEM_FONT,
};

enum
{
    QUIT,
    PROFILE_LIST_CHANGED,
    ENCODING_LIST_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum
{
    COL_PROFILE,
    NUM_COLUMNS
};

enum
{
    SOURCE_DEFAULT = 0,
    SOURCE_SESSION = 1
};

static TerminalApp *global_app = NULL;

#define MONOSPACE_FONT_SCHEMA "org.mate.interface"
#define MONOSPACE_FONT_KEY "monospace-font-name"
#define DEFAULT_MONOSPACE_FONT ("Monospace 10")

#define ENABLE_MNEMONICS_KEY "use-mnemonics"
#define DEFAULT_ENABLE_MNEMONICS (TRUE)

#define ENABLE_MENU_BAR_ACCEL_KEY "use-menu-accelerators"
#define DEFAULT_ENABLE_MENU_BAR_ACCEL (TRUE)

#define PROFILE_LIST_KEY "profile-list"
#define DEFAULT_PROFILE_KEY "default-profile"

#define ENCODING_LIST_KEY "active-encodings"


/* two following functions were copied from libmate-desktop to get rid
 * of dependency on it
 *
 * FIXME: I suspect there's no need for excessive copies, we might use
 * existing profile list to form GVariant's and write them to GSettings
 */
static gboolean
gsettings_append_strv (GSettings   *settings,
                            const gchar *key,
                            const gchar *value)
{
    gchar    **old;
    gchar    **new;
    gint       size;
    gboolean   retval;

    old = g_settings_get_strv (settings, key);

    for (size = 0; old[size] != NULL; size++);

    size += 1; /* appended value */
    size += 1; /* NULL */

    new = g_realloc_n (old, size, sizeof (gchar *));

    new[size - 2] = g_strdup (value);
    new[size - 1] = NULL;

    retval = g_settings_set_strv (settings, key,
                                  (const gchar **) new);

    g_strfreev (new);

    return retval;
}

static gboolean
gsettings_remove_all_from_strv (GSettings   *settings,
                                     const gchar *key,
                                     const gchar *value)
{
    GArray    *array;
    gchar    **old;
    gint       i;
    gboolean   retval;

    old = g_settings_get_strv (settings, key);
    array = g_array_new (TRUE, TRUE, sizeof (gchar *));

    for (i = 0; old[i] != NULL; i++) {
        if (g_strcmp0 (old[i], value) != 0)
            array = g_array_append_val (array, old[i]);
    }

    retval = g_settings_set_strv (settings, key,
                                  (const gchar **) array->data);

    g_strfreev (old);
    g_array_free (array, TRUE);

    return retval;
}

/* Helper functions */

static GdkScreen*
terminal_app_get_screen_by_display_name (const char *display_name)
{
	GdkDisplay *display = NULL;
	GdkScreen *screen = NULL;

	if (display_name == NULL)
		display = gdk_display_get_default ();
	else
	{
		GSList *displays, *l;
		const char *period;

		period = strrchr (display_name, '.');
		displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
		for (l = displays; l != NULL; l = l->next)
		{
			GdkDisplay *disp = l->data;

			/* compare without the screen number part, if present */
			if ((period && strncmp (gdk_display_get_name (disp), display_name, period - display_name) == 0) ||
			        (period == NULL && strcmp (gdk_display_get_name (disp), display_name) == 0))
			{
				display = disp;
				break;
			}
		}
		g_slist_free (displays);

		if (display == NULL)
			display = gdk_display_open (display_name); /* FIXME we never close displays */
	}

	if (display == NULL)
		return NULL;
	else
		screen = gdk_display_get_default_screen (display);

	return screen;
}

static int
terminal_app_get_workspace_for_window (TerminalWindow *window)
{
  int ret = -1;
  guchar *data = NULL;
  GdkAtom atom;
  GdkAtom cardinal_atom;

  atom = gdk_atom_intern_static_string ("_NET_WM_DESKTOP");
  cardinal_atom = gdk_atom_intern_static_string ("CARDINAL");

  gdk_property_get (gtk_widget_get_window(GTK_WIDGET(window)),
                    atom, cardinal_atom, 0, 8, FALSE,
		    NULL, NULL, NULL, &data);

  if (data)
    ret = *(int *)data;

  g_free (data);
  return ret;
}


/* Menubar mnemonics settings handling */

static int
profiles_alphabetic_cmp (gconstpointer pa,
                         gconstpointer pb)
{
	TerminalProfile *a = (TerminalProfile *) pa;
	TerminalProfile *b = (TerminalProfile *) pb;
	int result;

	result =  g_utf8_collate (terminal_profile_get_property_string (a, TERMINAL_PROFILE_VISIBLE_NAME),
	                          terminal_profile_get_property_string (b, TERMINAL_PROFILE_VISIBLE_NAME));
	if (result == 0)
		result = strcmp (terminal_profile_get_property_string (a, TERMINAL_PROFILE_NAME),
		                 terminal_profile_get_property_string (b, TERMINAL_PROFILE_NAME));

	return result;
}

typedef struct
{
	TerminalProfile *result;
	const char *target;
} LookupInfo;

static void
profiles_lookup_by_visible_name_foreach (gpointer key,
        gpointer value,
        gpointer data)
{
	LookupInfo *info = data;
	const char *name;

	name = terminal_profile_get_property_string (value, TERMINAL_PROFILE_VISIBLE_NAME);
	if (name && strcmp (info->target, name) == 0)
		info->result = value;
}

static void
terminal_window_destroyed (TerminalWindow *window,
                           TerminalApp    *app)
{
	app->windows = g_list_remove (app->windows, window);

	if (app->windows == NULL)
		g_signal_emit (app, signals[QUIT], 0);
}

static TerminalProfile *
terminal_app_create_profile (TerminalApp *app,
                             const char *name)
{
	TerminalProfile *profile;

	g_assert (terminal_app_get_profile_by_name (app, name) == NULL);

	profile = _terminal_profile_new (name);

	g_hash_table_insert (app->profiles,
	                     g_strdup (terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME)),
	                     profile /* adopts the refcount */);

	if (app->default_profile == NULL &&
	        app->default_profile_id != NULL &&
	        strcmp (app->default_profile_id,
	                terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME)) == 0)
	{
		/* We are the default profile */
		app->default_profile = profile;
		g_object_notify (G_OBJECT (app), TERMINAL_APP_DEFAULT_PROFILE);
	}

	return profile;
}

static void
terminal_app_delete_profile (TerminalProfile *profile)
{
	const char *profile_name;
	char *profile_dir;
	GError *error = NULL;

	profile_name = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME);
	profile_dir = g_strconcat (CONF_PROFILE_PREFIX, profile_name, "/", NULL);

	gsettings_remove_all_from_strv (settings_global, PROFILE_LIST_KEY, profile_name);

	/* And remove the profile directory */
	DConfClient *client = dconf_client_new ();
	if (!dconf_client_write_sync (client, profile_dir, NULL, NULL, NULL, &error))
	{
		g_warning ("Failed to recursively unset %s: %s\n", profile_dir, error->message);
		g_error_free (error);
	}

	g_object_unref (client);
	g_free (profile_dir);
}

static void
terminal_app_profile_cell_data_func (GtkTreeViewColumn *tree_column,
                                     GtkCellRenderer *cell,
                                     GtkTreeModel *tree_model,
                                     GtkTreeIter *iter,
                                     gpointer data)
{
	TerminalProfile *profile;
	GValue value = { 0, };

	gtk_tree_model_get (tree_model, iter, (int) COL_PROFILE, &profile, (int) -1);

	g_value_init (&value, G_TYPE_STRING);
	g_object_get_property (G_OBJECT (profile), "visible-name", &value);
	g_object_set_property (G_OBJECT (cell), "text", &value);
	g_value_unset (&value);
}

static int
terminal_app_profile_sort_func (GtkTreeModel *model,
                                GtkTreeIter *a,
                                GtkTreeIter *b,
                                gpointer user_data)
{
	TerminalProfile *profile_a, *profile_b;
	int retval;

	gtk_tree_model_get (model, a, (int) COL_PROFILE, &profile_a, (int) -1);
	gtk_tree_model_get (model, b, (int) COL_PROFILE, &profile_b, (int) -1);

	retval = profiles_alphabetic_cmp (profile_a, profile_b);

	g_object_unref (profile_a);
	g_object_unref (profile_b);

	return retval;
}

static /* ref */ GtkTreeModel *
terminal_app_get_profile_liststore (TerminalApp *app,
                                    TerminalProfile *selected_profile,
                                    GtkTreeIter *selected_profile_iter,
                                    gboolean *selected_profile_iter_set)
{
	GtkListStore *store;
	GtkTreeIter iter;
	GList *profiles, *l;

	store = gtk_list_store_new (NUM_COLUMNS, TERMINAL_TYPE_PROFILE);

	*selected_profile_iter_set = FALSE;

	if (selected_profile &&
	        _terminal_profile_get_forgotten (selected_profile))
		selected_profile = NULL;

	profiles = terminal_app_get_profile_list (app);

	for (l = profiles; l != NULL; l = l->next)
	{
		TerminalProfile *profile = TERMINAL_PROFILE (l->data);

		gtk_list_store_insert_with_values (store, &iter, 0,
		                                   (int) COL_PROFILE, profile,
		                                   (int) -1);

		if (selected_profile_iter && profile == selected_profile)
		{
			*selected_profile_iter = iter;
			*selected_profile_iter_set = TRUE;
		}
	}
	g_list_free (profiles);

	/* Now turn on sorting */
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (store),
	                                 COL_PROFILE,
	                                 terminal_app_profile_sort_func,
	                                 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
	                                      COL_PROFILE, GTK_SORT_ASCENDING);

	return GTK_TREE_MODEL (store);
}

static /* ref */ TerminalProfile*
profile_combo_box_get_selected (GtkWidget *widget)
{
	GtkComboBox *combo = GTK_COMBO_BOX (widget);
	TerminalProfile *profile = NULL;
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter (combo, &iter))
		gtk_tree_model_get (gtk_combo_box_get_model (combo), &iter,
		                    (int) COL_PROFILE, &profile, (int) -1);

	return profile;
}

static void
profile_combo_box_refill (TerminalApp *app,
                          GtkWidget *widget)
{
	GtkComboBox *combo = GTK_COMBO_BOX (widget);
	GtkTreeIter iter;
	gboolean iter_set;
	TerminalProfile *selected_profile;
	GtkTreeModel *model;

	selected_profile = profile_combo_box_get_selected (widget);
	if (!selected_profile)
	{
		selected_profile = terminal_app_get_default_profile (app);
		if (selected_profile)
			g_object_ref (selected_profile);
	}

	model = terminal_app_get_profile_liststore (app,
	        selected_profile,
	        &iter,
	        &iter_set);
	gtk_combo_box_set_model (combo, model);
	g_object_unref (model);

	if (iter_set)
		gtk_combo_box_set_active_iter (combo, &iter);

	if (selected_profile)
		g_object_unref (selected_profile);
}

static GtkWidget*
profile_combo_box_new (TerminalApp *app)
{
	GtkWidget *combo;
	GtkCellRenderer *renderer;

	combo = gtk_combo_box_new ();
	terminal_util_set_atk_name_description (combo, NULL, _("Click button to choose profile"));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (combo), renderer,
	                                    (GtkCellLayoutDataFunc) terminal_app_profile_cell_data_func,
	                                    NULL, NULL);

	profile_combo_box_refill (app, combo);
	g_signal_connect (app, "profile-list-changed",
	                  G_CALLBACK (profile_combo_box_refill), combo);

	gtk_widget_show (combo);
	return combo;
}

static void
profile_combo_box_changed_cb (GtkWidget *widget,
                              TerminalApp *app)
{
	TerminalProfile *profile;

	profile = profile_combo_box_get_selected (widget);
	if (!profile)
		return;

	g_settings_set_string (settings_global, DEFAULT_PROFILE_KEY,
			       terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME));

	/* Even though the GSettings change notification does this, it happens too late.
	 * In some cases, the default profile changes twice in quick succession,
	 * and update_default_profile must be called in sync with those changes.
	 */
	app->default_profile = profile;

	g_object_notify (G_OBJECT (app), TERMINAL_APP_DEFAULT_PROFILE);

	g_object_unref (profile);
}

static void
profile_list_treeview_refill (TerminalApp *app,
                              GtkWidget *widget)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
	GtkTreeIter iter;
	gboolean iter_set;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	TerminalProfile *selected_profile = NULL;

	model = gtk_tree_view_get_model (tree_view);

	selection = gtk_tree_view_get_selection (tree_view);
	if (gtk_tree_selection_get_selected (selection, NULL, &iter))
		gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

	model = terminal_app_get_profile_liststore (terminal_app_get (),
	        selected_profile,
	        &iter,
	        &iter_set);
	gtk_tree_view_set_model (tree_view, model);
	g_object_unref (model);

	if (!iter_set)
		iter_set = gtk_tree_model_get_iter_first (model, &iter);

	if (iter_set)
		gtk_tree_selection_select_iter (selection, &iter);

	if (selected_profile)
		g_object_unref (selected_profile);
}

static GtkWidget*
profile_list_treeview_create (TerminalApp *app)
{
	GtkWidget *tree_view;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	tree_view = gtk_tree_view_new ();
	terminal_util_set_atk_name_description (tree_view, _("Profile list"), NULL);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree_view), FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
	                             GTK_SELECTION_BROWSE);

	column = gtk_tree_view_column_new ();
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (column), renderer, TRUE);
	gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (column), renderer,
	                                    (GtkCellLayoutDataFunc) terminal_app_profile_cell_data_func,
	                                    NULL, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
	                             GTK_TREE_VIEW_COLUMN (column));

	return tree_view;
}

static void
profile_list_delete_confirm_response_cb (GtkWidget *dialog,
                                         int        response)
{
	TerminalProfile *profile;

	profile = TERMINAL_PROFILE (g_object_get_data (G_OBJECT (dialog), "profile"));
	g_assert (profile != NULL);

	if (response == GTK_RESPONSE_ACCEPT)
		terminal_app_delete_profile (profile);

	gtk_widget_destroy (dialog);
}

static void
mate_dialog_add_button (GtkDialog   *dialog,
                        const gchar *button_text,
                        const gchar *icon_name,
                        gint         response_id)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic (button_text);
	gtk_button_set_image (GTK_BUTTON (button), gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_BUTTON));

	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	gtk_style_context_add_class (gtk_widget_get_style_context (button), "text-button");
	gtk_widget_set_can_default (button, TRUE);
	gtk_widget_show (button);
	gtk_dialog_add_action_widget (GTK_DIALOG (dialog), button, response_id);
}

static void
profile_list_delete_button_clicked_cb (GtkWidget *button,
                                       GtkWidget *widget)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
	GtkTreeSelection *selection;
	GtkWidget *dialog;
	GtkTreeIter iter;
	GtkTreeModel *model;
	TerminalProfile *selected_profile;
	GtkWidget *transient_parent;

	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

	transient_parent = gtk_widget_get_toplevel (widget);
	dialog = gtk_message_dialog_new (GTK_WINDOW (transient_parent),
	                                 GTK_DIALOG_DESTROY_WITH_PARENT,
	                                 GTK_MESSAGE_QUESTION,
	                                 GTK_BUTTONS_NONE,
	                                 _("Delete profile “%s”?"),
	                                 terminal_profile_get_property_string (selected_profile, TERMINAL_PROFILE_VISIBLE_NAME));

	mate_dialog_add_button (GTK_DIALOG (dialog),
	                        _("_Cancel"),
	                        "process-stop",
	                        GTK_RESPONSE_REJECT);

	mate_dialog_add_button (GTK_DIALOG (dialog),
	                        _("_Delete"),
	                        "edit-delete",
	                        GTK_RESPONSE_ACCEPT);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
	                                 GTK_RESPONSE_ACCEPT);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Delete Profile"));
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

	/* Transfer refcount of |selected_profile|, so no unref below */
	g_object_set_data_full (G_OBJECT (dialog), "profile", selected_profile, g_object_unref);

	g_signal_connect (dialog, "response",
	                  G_CALLBACK (profile_list_delete_confirm_response_cb),
	                  NULL);

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
profile_list_new_button_clicked_cb (GtkWidget   *button,
                                    gpointer data)
{
	TerminalApp *app;

	app = terminal_app_get ();
	terminal_app_new_profile (app, NULL, GTK_WINDOW (app->manage_profiles_dialog));
}

static void
profile_list_edit_button_clicked_cb (GtkWidget *button,
                                     GtkWidget *widget)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW (widget);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreeModel *model;
	TerminalProfile *selected_profile;
	TerminalApp *app;

	app = terminal_app_get ();

	model = gtk_tree_view_get_model (tree_view);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

	terminal_app_edit_profile (app, selected_profile,
	                           GTK_WINDOW (app->manage_profiles_dialog),
	                           NULL);
	g_object_unref (selected_profile);
}

static void
profile_list_row_activated_cb (GtkTreeView       *tree_view,
                               GtkTreePath       *path,
                               GtkTreeViewColumn *column,
                               gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	TerminalProfile *selected_profile;
	TerminalApp *app;

	app = terminal_app_get ();

	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return;

	gtk_tree_model_get (model, &iter, (int) COL_PROFILE, &selected_profile, (int) -1);

	terminal_app_edit_profile (app, selected_profile,
	                           GTK_WINDOW (app->manage_profiles_dialog),
	                           NULL);
	g_object_unref (selected_profile);
}

static GList*
find_profile_link (GList      *profiles,
                   const char *name)
{
	GList *l;

	for (l = profiles; l != NULL; l = l->next)
	{
		const char *profile_name;

		profile_name = terminal_profile_get_property_string (TERMINAL_PROFILE (l->data), TERMINAL_PROFILE_NAME);
		if (profile_name && strcmp (profile_name, name) == 0)
			break;
	}

	return l;
}

static void
terminal_app_profile_list_notify_cb (GSettings   *settings,
                                     const gchar *key,
                                     gpointer     user_data)
{
	TerminalApp *app = TERMINAL_APP (user_data);
	GObject *object = G_OBJECT (app);
	GVariant *val;
	const gchar **value_list;
	int i;
	GList *profiles_to_delete, *l;
	gboolean need_new_default;
	TerminalProfile *fallback;
	guint count;

	g_object_freeze_notify (object);

	profiles_to_delete = terminal_app_get_profile_list (app);

	val = g_settings_get_value (settings, key);
	if (val == NULL ||
	        (!g_variant_is_of_type (val, G_VARIANT_TYPE_STRING_ARRAY) &&
	        !g_variant_is_of_type (val, G_VARIANT_TYPE_STRING)))
		goto ensure_one_profile;

	value_list = g_variant_get_strv (val, NULL);
	if (value_list == NULL)
		goto ensure_one_profile;

	/* Add any new ones */
	for (i = 0; value_list[i] != NULL; ++i)
	{
		const char *profile_name = value_list[i];
		GList *link;

		if (!profile_name)
			continue;

		link = find_profile_link (profiles_to_delete, profile_name);
		if (link)
			/* make profiles_to_delete point to profiles we didn't find in the list */
			profiles_to_delete = g_list_delete_link (profiles_to_delete, link);
		else
			terminal_app_create_profile (app, profile_name);
	}

	g_free (value_list);

ensure_one_profile:

	if (val != NULL)
		g_variant_unref (val);

	fallback = NULL;
	count = g_hash_table_size (app->profiles);
	if (count == 0 || count <= g_list_length (profiles_to_delete))
	{
		/* We are going to run out, so create the fallback
		 * to be sure we always have one. Must be done
		 * here before we emit "forgotten" signals so that
		 * screens have a profile to fall back to.
		 *
		 * If the profile with the FALLBACK_ID already exists,
		 * we aren't allowed to delete it, unless at least one
		 * other profile will still exist. And if you delete
		 * all profiles, the FALLBACK_ID profile returns as
		 * the living dead.
		 */
		fallback = terminal_app_get_profile_by_name (app, FALLBACK_PROFILE_ID);
		if (fallback == NULL)
			fallback = terminal_app_create_profile (app, FALLBACK_PROFILE_ID);
		g_assert (fallback != NULL);
	}

	/* Forget no-longer-existing profiles */
	need_new_default = FALSE;
	for (l = profiles_to_delete; l != NULL; l = l->next)
	{
		TerminalProfile *profile = TERMINAL_PROFILE (l->data);
		const char *name;

		name = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_NAME);
		if (strcmp (name, FALLBACK_PROFILE_ID) == 0)
			continue;

		if (profile == app->default_profile)
		{
			app->default_profile = NULL;
			need_new_default = TRUE;
		}

		_terminal_profile_forget (profile);
		g_hash_table_remove (app->profiles, name);

		/* |profile| possibly isn't dead yet since the profiles dialogue's tree model holds a ref too... */
	}
	g_list_free (profiles_to_delete);

	if (need_new_default)
	{
		TerminalProfile *new_default;
		TerminalProfile **new_default_ptr = &new_default;

		new_default = terminal_app_get_profile_by_name (app, FALLBACK_PROFILE_ID);
		if (new_default == NULL)
		{
			GHashTableIter iter;

			g_hash_table_iter_init (&iter, app->profiles);
			if (!g_hash_table_iter_next (&iter, NULL, (gpointer *) new_default_ptr))
				/* shouldn't really happen ever, but just to be safe */
				new_default = terminal_app_create_profile (app, FALLBACK_PROFILE_ID);
		}
		g_assert (new_default != NULL);

		app->default_profile = new_default;

		g_object_notify (object, TERMINAL_APP_DEFAULT_PROFILE);
	}

	g_assert (g_hash_table_size (app->profiles) > 0);

	g_signal_emit (app, signals[PROFILE_LIST_CHANGED], 0);

	g_object_thaw_notify (object);
}

static void
terminal_app_default_profile_notify_cb (GSettings   *settings,
                                        const gchar *key,
                                        gpointer     user_data)
{
	TerminalApp *app = TERMINAL_APP (user_data);
	GVariant *val;
	const char *name = NULL;

	app->default_profile_locked = !g_settings_is_writable (settings, key);

	val = g_settings_get_value (settings, key);
	if (val != NULL &&
	        g_variant_is_of_type (val, G_VARIANT_TYPE_STRING))
		name = g_variant_get_string (val, NULL);
	if (!name || !name[0])
		name = FALLBACK_PROFILE_ID;
	g_assert (name != NULL);

	g_free (app->default_profile_id);
	app->default_profile_id = g_strdup (name);

	app->default_profile = terminal_app_get_profile_by_name (app, name);

	g_object_notify (G_OBJECT (app), TERMINAL_APP_DEFAULT_PROFILE);
	g_variant_unref (val);
}

static int
compare_encodings (TerminalEncoding *a,
                   TerminalEncoding *b)
{
	return g_utf8_collate (a->name, b->name);
}

static void
encoding_mark_active (gpointer key,
                      gpointer value,
                      gpointer data)
{
	TerminalEncoding *encoding = (TerminalEncoding *) value;
	guint active = GPOINTER_TO_UINT (data);

	encoding->is_active = active;
}

static void
terminal_app_encoding_list_notify_cb (GSettings   *settings,
                                      const gchar *key,
                                      gpointer     user_data)
{
	TerminalApp *app = TERMINAL_APP (user_data);
	GVariant *val;
	const gchar **strings;
	int i;
	TerminalEncoding *encoding;
	const char *charset;

	app->encodings_locked = !g_settings_is_writable (settings, key);

	/* Mark all as non-active, then re-enable the active ones */
	g_hash_table_foreach (app->encodings, (GHFunc) encoding_mark_active, GUINT_TO_POINTER (FALSE));

	/* First add the locale's charset */
	encoding = g_hash_table_lookup (app->encodings, "current");
	g_assert (encoding);
	if (terminal_encoding_is_valid (encoding))
		encoding->is_active = TRUE;

	/* Also always make UTF-8 available */
	encoding = g_hash_table_lookup (app->encodings, "UTF-8");
	g_assert (encoding);
	if (terminal_encoding_is_valid (encoding))
		encoding->is_active = TRUE;

	val = g_settings_get_value (settings, key);
	if (val != NULL &&
	        g_variant_is_of_type (val, G_VARIANT_TYPE_STRING_ARRAY))
		strings = g_variant_get_strv (val, NULL);
	else
		strings = NULL;

	if (strings != NULL)
	{
		for (i = 0; strings[i] != NULL; ++i)
		{
			charset = strings[i];
			if (!charset)
				continue;

			encoding = terminal_app_ensure_encoding (app, charset);
			if (!terminal_encoding_is_valid (encoding))
				continue;

			encoding->is_active = TRUE;
		}

		g_free (strings);
	}

	g_signal_emit (app, signals[ENCODING_LIST_CHANGED], 0);

	if (val != NULL)
		g_variant_unref (val);
}

static void
terminal_app_system_font_notify_cb (GSettings   *settings,
                                    const gchar *key,
                                    gpointer     user_data)
{
	TerminalApp *app = TERMINAL_APP (user_data);
	GVariant *val;
	const char *font = NULL;
	PangoFontDescription *font_desc;

	if (strcmp (key, MONOSPACE_FONT_KEY) != 0)
		return;

	val = g_settings_get_value (settings, key);
	if (val &&
	        g_variant_is_of_type (val, G_VARIANT_TYPE_STRING))
		font = g_variant_get_string (val, NULL);
	if (!font)
		font = DEFAULT_MONOSPACE_FONT;
	g_assert (font != NULL);

	if (font && (strlen (font) == 0))   /* empty string */
		font = DEFAULT_MONOSPACE_FONT;

	font_desc = pango_font_description_from_string (font);
	if (app->system_font_desc &&
	        pango_font_description_equal (app->system_font_desc, font_desc))
	{
		pango_font_description_free (font_desc);
		return;
	}

	if (app->system_font_desc)
		pango_font_description_free (app->system_font_desc);

	app->system_font_desc = font_desc;

	g_object_notify (G_OBJECT (app), TERMINAL_APP_SYSTEM_FONT);
	g_variant_unref (val);
}

static void
terminal_app_enable_mnemonics_notify_cb (GSettings   *settings,
                                         const gchar *key,
                                         gpointer     user_data)
{
	TerminalApp *app = TERMINAL_APP (user_data);
	gboolean enable;

	enable = g_settings_get_boolean (settings, key);
	if (enable == app->enable_mnemonics)
		return;

	app->enable_mnemonics = enable;
	g_object_notify (G_OBJECT (app), TERMINAL_APP_ENABLE_MNEMONICS);
}

static void
terminal_app_enable_menu_accels_notify_cb (GSettings   *settings,
                                           const gchar *key,
                                           gpointer     user_data)
{
	TerminalApp *app = TERMINAL_APP (user_data);
	gboolean enable;

	enable = g_settings_get_boolean (settings, key);
	if (enable == app->enable_menu_accels)
		return;

	app->enable_menu_accels = enable;
	g_object_notify (G_OBJECT (app), TERMINAL_APP_ENABLE_MENU_BAR_ACCEL);
}

static void
new_profile_response_cb (GtkWidget *new_profile_dialog,
                         int        response_id,
                         TerminalApp *app)
{
	if (response_id == GTK_RESPONSE_ACCEPT)
	{
		GtkWidget *name_entry;
		char *name;
		const char *new_profile_name;
		GtkWidget *base_option_menu;
		TerminalProfile *base_profile = NULL;
		TerminalProfile *new_profile;
		GList *profiles;
		GList *tmp;
		GtkWindow *transient_parent;
		GtkWidget *confirm_dialog;
		gint retval;

		base_option_menu = g_object_get_data (G_OBJECT (new_profile_dialog), "base_option_menu");
		base_profile = profile_combo_box_get_selected (base_option_menu);
		if (!base_profile)
			base_profile = terminal_app_get_default_profile (app);
		if (!base_profile)
			return; /* shouldn't happen ever though */

		name_entry = g_object_get_data (G_OBJECT (new_profile_dialog), "name_entry");
		name = gtk_editable_get_chars (GTK_EDITABLE (name_entry), 0, -1);
		g_strstrip (name); /* name will be non empty after stripping */

		profiles = terminal_app_get_profile_list (app);
		for (tmp = profiles; tmp != NULL; tmp = tmp->next)
		{
			TerminalProfile *profile = tmp->data;
			const char *visible_name;

			visible_name = terminal_profile_get_property_string (profile, TERMINAL_PROFILE_VISIBLE_NAME);

			if (visible_name && strcmp (name, visible_name) == 0)
				break;
		}
		if (tmp)
		{
			confirm_dialog = gtk_message_dialog_new (GTK_WINDOW (new_profile_dialog),
			                 GTK_DIALOG_DESTROY_WITH_PARENT,
			                 GTK_MESSAGE_QUESTION,
			                 GTK_BUTTONS_YES_NO,
			                 _("You already have a profile called “%s”. Do you want to create another profile with the same name?"), name);
			/* Alternative button order was set automatically by GtkMessageDialog */
			retval = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
			gtk_widget_destroy (confirm_dialog);
			if (retval == GTK_RESPONSE_NO)
				goto cleanup;
		}
		g_list_free (profiles);

		transient_parent = gtk_window_get_transient_for (GTK_WINDOW (new_profile_dialog));

		new_profile = _terminal_profile_clone (base_profile, name);
		new_profile_name = terminal_profile_get_property_string (new_profile, TERMINAL_PROFILE_NAME);
		g_hash_table_insert (app->profiles,
		                     g_strdup (new_profile_name),
		                     new_profile /* adopts the refcount */);

		/* And now save the new profile name to GSettings */
		gsettings_append_strv (settings_global,
						PROFILE_LIST_KEY,
						new_profile_name);

		terminal_profile_edit (new_profile, transient_parent, NULL);

cleanup:
		g_free (name);
	}

	gtk_widget_destroy (new_profile_dialog);
}

static void
new_profile_dialog_destroy_cb (GtkWidget *new_profile_dialog,
                               TerminalApp *app)
{
	GtkWidget *combo;

	combo = g_object_get_data (G_OBJECT (new_profile_dialog), "base_option_menu");
	g_signal_handlers_disconnect_by_func (app, G_CALLBACK (profile_combo_box_refill), combo);

	app->new_profile_dialog = NULL;
}

static void
new_profile_name_entry_changed_cb (GtkEntry *entry,
                                   GtkDialog *dialog)
{
	const char *name;

	name = gtk_entry_get_text (entry);

	/* make the create button sensitive only if something other than space has been set */
	while (*name != '\0' && g_ascii_isspace (*name))
		++name;

	gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_ACCEPT, name[0] != '\0');
}

void
terminal_app_new_profile (TerminalApp     *app,
                          TerminalProfile *default_base_profile,
                          GtkWindow       *transient_parent)
{
	if (app->new_profile_dialog == NULL)
	{
		GtkWidget *create_button, *grid, *name_label, *name_entry, *base_label, *combo;

		if (!terminal_util_load_builder_resource (TERMINAL_RESOURCES_PATH_PREFIX G_DIR_SEPARATOR_S "ui/profile-new-dialog.ui",
		                                      "new-profile-dialog", &app->new_profile_dialog,
		                                      "new-profile-create-button", &create_button,
		                                      "new-profile-grid", &grid,
		                                      "new-profile-name-label", &name_label,
		                                      "new-profile-name-entry", &name_entry,
		                                      "new-profile-base-label", &base_label,
		                                      NULL))
			return;

		g_signal_connect (G_OBJECT (app->new_profile_dialog), "response", G_CALLBACK (new_profile_response_cb), app);
		g_signal_connect (app->new_profile_dialog, "destroy", G_CALLBACK (new_profile_dialog_destroy_cb), app);

		g_object_set_data (G_OBJECT (app->new_profile_dialog), "create_button", create_button);
		gtk_widget_set_sensitive (create_button, FALSE);

		/* the name entry */
		g_object_set_data (G_OBJECT (app->new_profile_dialog), "name_entry", name_entry);
		g_signal_connect (name_entry, "changed", G_CALLBACK (new_profile_name_entry_changed_cb), app->new_profile_dialog);
		gtk_entry_set_activates_default (GTK_ENTRY (name_entry), TRUE);
		gtk_widget_grab_focus (name_entry);

		gtk_label_set_mnemonic_widget (GTK_LABEL (name_label), name_entry);

		/* the base profile option menu */
		combo = profile_combo_box_new (app);
		gtk_grid_attach (GTK_GRID (grid), combo, 2, 1, 1, 1);
		g_object_set_data (G_OBJECT (app->new_profile_dialog), "base_option_menu", combo);
		terminal_util_set_atk_name_description (combo, NULL, _("Choose base profile"));

		gtk_label_set_mnemonic_widget (GTK_LABEL (base_label), combo);

		gtk_dialog_set_default_response (GTK_DIALOG (app->new_profile_dialog), GTK_RESPONSE_ACCEPT);
		gtk_dialog_set_response_sensitive (GTK_DIALOG (app->new_profile_dialog), GTK_RESPONSE_ACCEPT, FALSE);
	}

	gtk_window_set_transient_for (GTK_WINDOW (app->new_profile_dialog),
	                              transient_parent);

	gtk_window_present (GTK_WINDOW (app->new_profile_dialog));
}

static void
profile_list_selection_changed_cb (GtkTreeSelection *selection,
                                   TerminalApp *app)
{
	gboolean selected;

	selected = gtk_tree_selection_get_selected (selection, NULL, NULL);

	gtk_widget_set_sensitive (app->manage_profiles_edit_button, selected);
	gtk_widget_set_sensitive (app->manage_profiles_delete_button,
	                          selected &&
	                          g_hash_table_size (app->profiles) > 1);
}

static void
profile_list_response_cb (GtkWidget *dialog,
                          int        id,
                          TerminalApp *app)
{
	g_assert (app->manage_profiles_dialog == dialog);

	if (id == GTK_RESPONSE_HELP)
	{
		terminal_util_show_help ("mate-terminal-manage-profiles", GTK_WINDOW (dialog));
		return;
	}

	gtk_widget_destroy (dialog);
}

static void
profile_list_destroyed_cb (GtkWidget   *manage_profiles_dialog,
                           TerminalApp *app)
{
	g_signal_handlers_disconnect_by_func (app, G_CALLBACK (profile_list_treeview_refill), app->manage_profiles_list);
	g_signal_handlers_disconnect_by_func (app, G_CALLBACK (profile_combo_box_refill), app->manage_profiles_default_menu);

	app->manage_profiles_dialog = NULL;
	app->manage_profiles_list = NULL;
	app->manage_profiles_new_button = NULL;
	app->manage_profiles_edit_button = NULL;
	app->manage_profiles_delete_button = NULL;
	app->manage_profiles_default_menu = NULL;
}

void
terminal_app_manage_profiles (TerminalApp     *app,
                              GtkWindow       *transient_parent)
{
	GObject *dialog;
	GObject *tree_view_container, *new_button, *edit_button, *remove_button;
	GObject *default_hbox, *default_label;
	GtkTreeSelection *selection;

	if (app->manage_profiles_dialog)
	{
		gtk_window_set_transient_for (GTK_WINDOW (app->manage_profiles_dialog), transient_parent);
		gtk_window_present (GTK_WINDOW (app->manage_profiles_dialog));
		return;
	}

	if (!terminal_util_load_builder_resource (TERMINAL_RESOURCES_PATH_PREFIX G_DIR_SEPARATOR_S "ui/profile-manager.ui",
	                                      "profile-manager", &dialog,
	                                      "profiles-treeview-container", &tree_view_container,
	                                      "new-profile-button", &new_button,
	                                      "edit-profile-button", &edit_button,
	                                      "delete-profile-button", &remove_button,
	                                      "default-profile-hbox", &default_hbox,
	                                      "default-profile-label", &default_label,
	                                      NULL))
		return;

	app->manage_profiles_dialog = GTK_WIDGET (dialog);
	app->manage_profiles_new_button = GTK_WIDGET (new_button);
	app->manage_profiles_edit_button = GTK_WIDGET (edit_button);
	app->manage_profiles_delete_button  = GTK_WIDGET (remove_button);

	g_signal_connect (dialog, "response", G_CALLBACK (profile_list_response_cb), app);
	g_signal_connect (dialog, "destroy", G_CALLBACK (profile_list_destroyed_cb), app);

	app->manage_profiles_list = profile_list_treeview_create (app);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (app->manage_profiles_list));
	g_signal_connect (selection, "changed", G_CALLBACK (profile_list_selection_changed_cb), app);

	profile_list_treeview_refill (app, app->manage_profiles_list);
	g_signal_connect (app, "profile-list-changed",
	                  G_CALLBACK (profile_list_treeview_refill), app->manage_profiles_list);

	g_signal_connect (app->manage_profiles_list, "row-activated",
	                  G_CALLBACK (profile_list_row_activated_cb), app);

	gtk_container_add (GTK_CONTAINER (tree_view_container), app->manage_profiles_list);
	gtk_widget_show (app->manage_profiles_list);

	g_signal_connect (new_button, "clicked",
	                  G_CALLBACK (profile_list_new_button_clicked_cb),
	                  app->manage_profiles_list);
	g_signal_connect (edit_button, "clicked",
	                  G_CALLBACK (profile_list_edit_button_clicked_cb),
	                  app->manage_profiles_list);
	g_signal_connect (remove_button, "clicked",
	                  G_CALLBACK (profile_list_delete_button_clicked_cb),
	                  app->manage_profiles_list);

	app->manage_profiles_default_menu = profile_combo_box_new (app);
	g_signal_connect (app->manage_profiles_default_menu, "changed",
	                  G_CALLBACK (profile_combo_box_changed_cb), app);

	gtk_box_pack_start (GTK_BOX (default_hbox), app->manage_profiles_default_menu, FALSE, FALSE, 0);
	gtk_widget_show (app->manage_profiles_default_menu);

	gtk_label_set_mnemonic_widget (GTK_LABEL (default_label), app->manage_profiles_default_menu);

	gtk_widget_grab_focus (app->manage_profiles_list);

	gtk_window_set_transient_for (GTK_WINDOW (app->manage_profiles_dialog),
	                              transient_parent);

	gtk_window_present (GTK_WINDOW (app->manage_profiles_dialog));
}

#ifdef HAVE_SMCLIENT
static void
terminal_app_save_state_cb (EggSMClient *client,
                            GKeyFile *key_file,
                            TerminalApp *app)
{
	terminal_app_save_config (app, key_file);
}

static void
terminal_app_client_quit_cb (EggSMClient *client,
                             TerminalApp *app)
{
	g_signal_emit (app, signals[QUIT], 0);
}
#endif /* HAVE_SMCLIENT */

/* Class implementation */

G_DEFINE_TYPE (TerminalApp, terminal_app, G_TYPE_OBJECT)

static void
terminal_app_init (TerminalApp *app)
{
	global_app = app;

	gtk_window_set_default_icon_name (MATE_TERMINAL_ICON_NAME);

	/* Initialise defaults */
	app->enable_mnemonics = DEFAULT_ENABLE_MNEMONICS;
	app->enable_menu_accels = DEFAULT_ENABLE_MENU_BAR_ACCEL;

	app->profiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

	app->encodings = terminal_encodings_get_builtins ();

	settings_global = g_settings_new (CONF_GLOBAL_SCHEMA);
	app->settings_font = g_settings_new (MONOSPACE_FONT_SCHEMA);

	g_signal_connect (settings_global,
			  "changed::" PROFILE_LIST_KEY,
			  G_CALLBACK(terminal_app_profile_list_notify_cb),
			  app);

	g_signal_connect (settings_global,
			  "changed::" DEFAULT_PROFILE_KEY,
			  G_CALLBACK(terminal_app_default_profile_notify_cb),
			  app);

	g_signal_connect (settings_global,
			  "changed::" ENCODING_LIST_KEY,
			  G_CALLBACK(terminal_app_encoding_list_notify_cb),
			  app);

	g_signal_connect (app->settings_font,
			  "changed::" MONOSPACE_FONT_KEY,
			  G_CALLBACK(terminal_app_system_font_notify_cb),
			  app);


	g_signal_connect (settings_global,
	                  "changed::" ENABLE_MNEMONICS_KEY,
	                  G_CALLBACK(terminal_app_enable_mnemonics_notify_cb),
	                  app);

	g_signal_connect (settings_global,
	                  "changed::" ENABLE_MENU_BAR_ACCEL_KEY,
	                  G_CALLBACK(terminal_app_enable_menu_accels_notify_cb),
	                  app);

	/* Load the settings */
        terminal_app_profile_list_notify_cb (settings_global,
					     PROFILE_LIST_KEY,
					     app);
	terminal_app_default_profile_notify_cb (settings_global,
					        DEFAULT_PROFILE_KEY,
						app);
	terminal_app_encoding_list_notify_cb (settings_global,
					      ENCODING_LIST_KEY,
					      app);
	terminal_app_system_font_notify_cb (app->settings_font,
					    MONOSPACE_FONT_KEY,
					    app);
	terminal_app_enable_menu_accels_notify_cb (settings_global,
	                                           ENABLE_MENU_BAR_ACCEL_KEY,
	                                           app);
	terminal_app_enable_mnemonics_notify_cb (settings_global,
	                                         ENABLE_MNEMONICS_KEY,
	                                         app);

	/* Ensure we have valid settings */
	g_assert (app->default_profile_id != NULL);
	g_assert (app->system_font_desc != NULL);

	terminal_accels_init ();

#ifdef HAVE_SMCLIENT
	EggSMClient *sm_client;
	char *desktop_file;

	desktop_file = g_build_filename (TERM_DATADIR,
	                                 "applications",
	                                 PACKAGE ".desktop",
	                                 NULL);
	egg_set_desktop_file_without_defaults (desktop_file);
	g_free (desktop_file);

	sm_client = egg_sm_client_get ();
	g_signal_connect (sm_client, "save-state",
	                  G_CALLBACK (terminal_app_save_state_cb), app);
	g_signal_connect (sm_client, "quit",
	                  G_CALLBACK (terminal_app_client_quit_cb), app);
#endif /* HAVE_SMCLIENT */
}

static void
terminal_app_finalize (GObject *object)
{
	TerminalApp *app = TERMINAL_APP (object);

#ifdef HAVE_SMCLIENT
	EggSMClient *sm_client;

	sm_client = egg_sm_client_get ();
	g_signal_handlers_disconnect_matched (sm_client, G_SIGNAL_MATCH_DATA,
	                                      0, 0, NULL, NULL, app);
#endif /* HAVE_SMCLIENT */

	g_signal_handlers_disconnect_by_func (settings_global,
					      G_CALLBACK(terminal_app_profile_list_notify_cb),
					      app);
	g_signal_handlers_disconnect_by_func (settings_global,
					      G_CALLBACK(terminal_app_default_profile_notify_cb),
					      app);
	g_signal_handlers_disconnect_by_func (settings_global,
					      G_CALLBACK(terminal_app_encoding_list_notify_cb),
					      app);
	g_signal_handlers_disconnect_by_func (app->settings_font,
					      G_CALLBACK(terminal_app_system_font_notify_cb),
					      app);
	g_signal_handlers_disconnect_by_func (settings_global,
	                                      G_CALLBACK(terminal_app_enable_menu_accels_notify_cb),
	                                      app);
	g_signal_handlers_disconnect_by_func (settings_global,
	                                      G_CALLBACK(terminal_app_enable_mnemonics_notify_cb),
	                                      app);

	g_object_unref (settings_global);
	g_object_unref (app->settings_font);

	g_free (app->default_profile_id);

	g_hash_table_destroy (app->profiles);

	g_hash_table_destroy (app->encodings);

	pango_font_description_free (app->system_font_desc);

	terminal_accels_shutdown ();

	G_OBJECT_CLASS (terminal_app_parent_class)->finalize (object);

	global_app = NULL;
}

static void
terminal_app_get_property (GObject *object,
                           guint prop_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	TerminalApp *app = TERMINAL_APP (object);

	switch (prop_id)
	{
	case PROP_SYSTEM_FONT:
		if (app->system_font_desc)
			g_value_set_boxed (value, app->system_font_desc);
		else
			g_value_take_boxed (value, pango_font_description_from_string (DEFAULT_MONOSPACE_FONT));
		break;
	case PROP_ENABLE_MENU_BAR_ACCEL:
		g_value_set_boolean (value, app->enable_menu_accels);
		break;
	case PROP_ENABLE_MNEMONICS:
		g_value_set_boolean (value, app->enable_mnemonics);
		break;
	case PROP_DEFAULT_PROFILE:
		g_value_set_object (value, app->default_profile);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
terminal_app_set_property (GObject *object,
                           guint prop_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	TerminalApp *app = TERMINAL_APP (object);

	switch (prop_id)
	{
	case PROP_ENABLE_MENU_BAR_ACCEL:
		app->enable_menu_accels = g_value_get_boolean (value);
		g_settings_set_boolean (settings_global, ENABLE_MENU_BAR_ACCEL_KEY, app->enable_menu_accels);
		break;
	case PROP_ENABLE_MNEMONICS:
		app->enable_mnemonics = g_value_get_boolean (value);
		g_settings_set_boolean (settings_global, ENABLE_MNEMONICS_KEY, app->enable_mnemonics);
		break;
	case PROP_DEFAULT_PROFILE:
	case PROP_SYSTEM_FONT:
		/* not writable */
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
terminal_app_real_quit (TerminalApp *app)
{
	gtk_main_quit();
}

static void
terminal_app_class_init (TerminalAppClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = terminal_app_finalize;
	object_class->get_property = terminal_app_get_property;
	object_class->set_property = terminal_app_set_property;

	klass->quit = terminal_app_real_quit;

	signals[QUIT] =
	    g_signal_new (I_("quit"),
	                  G_OBJECT_CLASS_TYPE (object_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (TerminalAppClass, quit),
	                  NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID,
	                  G_TYPE_NONE, 0);

	signals[PROFILE_LIST_CHANGED] =
	    g_signal_new (I_("profile-list-changed"),
	                  G_OBJECT_CLASS_TYPE (object_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (TerminalAppClass, profile_list_changed),
	                  NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID,
	                  G_TYPE_NONE, 0);

	signals[ENCODING_LIST_CHANGED] =
	    g_signal_new (I_("encoding-list-changed"),
	                  G_OBJECT_CLASS_TYPE (object_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (TerminalAppClass, profile_list_changed),
	                  NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID,
	                  G_TYPE_NONE, 0);

	g_object_class_install_property
	(object_class,
	 PROP_ENABLE_MENU_BAR_ACCEL,
	 g_param_spec_boolean (TERMINAL_APP_ENABLE_MENU_BAR_ACCEL, NULL, NULL,
	                       DEFAULT_ENABLE_MENU_BAR_ACCEL,
	                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property
	(object_class,
	 PROP_ENABLE_MNEMONICS,
	 g_param_spec_boolean (TERMINAL_APP_ENABLE_MNEMONICS, NULL, NULL,
	                       DEFAULT_ENABLE_MNEMONICS,
	                       G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property
	(object_class,
	 PROP_SYSTEM_FONT,
	 g_param_spec_boxed (TERMINAL_APP_SYSTEM_FONT, NULL, NULL,
	                     PANGO_TYPE_FONT_DESCRIPTION,
	                     G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

	g_object_class_install_property
	(object_class,
	 PROP_DEFAULT_PROFILE,
	 g_param_spec_object (TERMINAL_APP_DEFAULT_PROFILE, NULL, NULL,
	                      TERMINAL_TYPE_PROFILE,
	                      G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

/* Public API */

TerminalApp*
terminal_app_get (void)
{
	if (global_app == NULL)
	{
		g_object_new (TERMINAL_TYPE_APP, NULL);
		g_assert (global_app != NULL);
	}

	return global_app;
}

void
terminal_app_shutdown (void)
{
	if (global_app == NULL)
		return;

	g_object_unref (global_app);
	g_assert (global_app == NULL);
}

/**
 * terminal_app_handle_options:
 * @app:
 * @options: a #TerminalOptions
 * @allow_resume: whether to merge the terminal configuration from the
 *   saved session on resume
 * @error: a #GError to fill in
 *
 * Processes @options. It loads or saves the terminal configuration, or
 * opens the specified windows and tabs.
 *
 * Returns: %TRUE if @options could be successfully handled, or %FALSE on
 *   error
 */
gboolean
terminal_app_handle_options (TerminalApp *app,
                             TerminalOptions *options,
                             gboolean allow_resume,
                             GError **error)
{
	GList *lw;
	GdkScreen *gdk_screen;

	gdk_screen = terminal_app_get_screen_by_display_name (options->display_name);

	if (options->save_config)
	{
		if (options->remote_arguments)
			return terminal_app_save_config_file (app, options->config_file, error);

		g_set_error_literal (error, TERMINAL_OPTION_ERROR, TERMINAL_OPTION_ERROR_NOT_IN_FACTORY,
		                     "Cannot use \"--save-config\" when starting the factory process");
		return FALSE;
	}

	if (options->load_config)
	{
		GKeyFile *key_file;
		gboolean result;

		key_file = g_key_file_new ();
		result = g_key_file_load_from_file (key_file, options->config_file, 0, error) &&
		         terminal_options_merge_config (options, key_file, SOURCE_DEFAULT, error);
		g_key_file_free (key_file);

		if (!result)
			return FALSE;

		/* fall-through on success */
	}

#ifdef HAVE_SMCLIENT
	EggSMClient *sm_client;

	sm_client = egg_sm_client_get ();

	if (allow_resume && egg_sm_client_is_resumed (sm_client))
	{
		GKeyFile *key_file;

		key_file = egg_sm_client_get_state_file (sm_client);
		if (key_file != NULL &&
		        !terminal_options_merge_config (options, key_file, SOURCE_SESSION, error))
			return FALSE;
	}
#endif /* HAVE_SMCLIENT */

	/* Make sure we open at least one window */
	terminal_options_ensure_window (options);

	if (options->startup_id != NULL)
		_terminal_debug_print (TERMINAL_DEBUG_FACTORY,
		                       "Startup ID is '%s'\n",
		                       options->startup_id);

	for (lw = options->initial_windows;  lw != NULL; lw = lw->next)
	{
		InitialWindow *iw = lw->data;
		TerminalWindow *window = NULL;
		GList *lt;

		g_assert (iw->tabs);

        if ( lw == options->initial_windows && ((InitialTab *)iw->tabs->data)->attach_window )
            window = terminal_app_get_current_window(app, gdk_screen, options->initial_workspace);

        if (!window)
        {
            /* Create & setup new window */
            window = terminal_app_new_window (app, gdk_screen);

            /* Restored windows shouldn't demand attention; see bug #586308. */
            if (iw->source_tag == SOURCE_SESSION)
                terminal_window_set_is_restored (window);

            if (options->startup_id != NULL)
                gtk_window_set_startup_id (GTK_WINDOW (window), options->startup_id);

            /* Overwrite the default, unique window role set in terminal_window_init */
            if (iw->role)
                gtk_window_set_role (GTK_WINDOW (window), iw->role);

            if (iw->force_menubar_state)
                terminal_window_set_menubar_visible (window, iw->menubar_state);

            if (iw->start_fullscreen)
                gtk_window_fullscreen (GTK_WINDOW (window));
            if (iw->start_maximized)
                gtk_window_maximize (GTK_WINDOW (window));
        }

		/* Now add the tabs */
		for (lt = iw->tabs; lt != NULL; lt = lt->next)
		{
			InitialTab *it = lt->data;
			TerminalProfile *profile = NULL;
			TerminalScreen *screen;
			const char *profile_name;
			gboolean profile_is_id;

			if (it->profile)
			{
				profile_name = it->profile;
				profile_is_id = it->profile_is_id;
			}
			else
			{
				profile_name = options->default_profile;
				profile_is_id = options->default_profile_is_id;
			}

			if (profile_name)
			{
				if (profile_is_id)
					profile = terminal_app_get_profile_by_name (app, profile_name);
				else
					profile = terminal_app_get_profile_by_visible_name (app, profile_name);

				if (profile == NULL)
					g_printerr (_("No such profile \"%s\", using default profile\n"), it->profile);
			}
			if (profile == NULL)
				profile = terminal_app_get_profile_for_new_term (app);
			g_assert (profile);

			screen = terminal_app_new_terminal (app, window, profile,
			                                    it->exec_argv ? it->exec_argv : options->exec_argv,
			                                    it->title ? it->title : options->default_title,
			                                    it->working_dir ? it->working_dir : options->default_working_dir,
			                                    options->env,
			                                    it->zoom_set ? it->zoom : options->zoom);

			if (it->active)
				terminal_window_switch_screen (window, screen);
		}

		if (iw->geometry)
		{
			_terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
			                       "[window %p] applying geometry %s\n",
			                       window, iw->geometry);

			if (!terminal_window_update_size_set_geometry (window,
			                                               terminal_window_get_active (window),
			                                               FALSE,
			                                               iw->geometry))
				g_printerr (_("Invalid geometry string \"%s\"\n"), iw->geometry);
		}

		gtk_window_present (GTK_WINDOW (window));
	}

	return TRUE;
}

TerminalWindow *
terminal_app_new_window (TerminalApp *app,
                         GdkScreen *screen)
{
	TerminalWindow *window;

	window = terminal_window_new ();

	app->windows = g_list_append (app->windows, window);
	g_signal_connect (window, "destroy",
	                  G_CALLBACK (terminal_window_destroyed), app);

	if (screen)
		gtk_window_set_screen (GTK_WINDOW (window), screen);

	return window;
}

TerminalScreen *
terminal_app_new_terminal (TerminalApp     *app,
                           TerminalWindow  *window,
                           TerminalProfile *profile,
                           char           **override_command,
                           const char      *title,
                           const char      *working_dir,
                           char           **child_env,
                           double           zoom)
{
	TerminalScreen *screen;

	g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
	g_return_val_if_fail (TERMINAL_IS_WINDOW (window), NULL);

	screen = terminal_screen_new (profile, override_command, title,
	                              working_dir, child_env, zoom);

	terminal_window_add_screen (window, screen, -1);
	terminal_window_switch_screen (window, screen);
	gtk_widget_grab_focus (GTK_WIDGET (screen));

	return screen;
}

void
terminal_app_edit_profile (TerminalApp     *app,
                           TerminalProfile *profile,
                           GtkWindow       *transient_parent,
                           const char      *widget_name)
{
	terminal_profile_edit (profile, transient_parent, widget_name);
}

void
terminal_app_edit_keybindings (TerminalApp     *app,
                               GtkWindow       *transient_parent)
{
	terminal_edit_keys_dialog_show (transient_parent);
}

void
terminal_app_edit_encodings (TerminalApp     *app,
                             GtkWindow       *transient_parent)
{
	terminal_encoding_dialog_show (transient_parent);
}

/*
* Get the window in the given screen and workspace. If nothing is found,
* a NULL is returned.
*/
TerminalWindow *
terminal_app_get_current_window (TerminalApp *app,
                                 GdkScreen *from_screen,
                                 int workspace)
{
    GList *res = NULL;
    TerminalWindow *ret = NULL;

	if (app->windows == NULL)
		return NULL;

    res = g_list_last (app->windows);

    g_assert (from_screen != NULL);

    while (res)
    {
      int win_workspace;
      if (gtk_window_get_screen(GTK_WINDOW(res->data)) != from_screen)
        continue;

      win_workspace = terminal_app_get_workspace_for_window(res->data);

      /* Same workspace or if the window is set to show up on all workspaces */
      if (win_workspace == workspace || win_workspace == -1)
        ret = terminal_window_get_latest_focused (ret, TERMINAL_WINDOW(res->data));

      res = g_list_previous (res);
    }

  return ret;
}

/**
 * terminal_profile_get_list:
 *
 * Returns: a #GList containing all #TerminalProfile objects.
 *   The content of the list is owned by the backend and
 *   should not be modified or freed. Use g_list_free() when done
 *   using the list.
 */
GList*
terminal_app_get_profile_list (TerminalApp *app)
{
	g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

	return g_list_sort (g_hash_table_get_values (app->profiles), profiles_alphabetic_cmp);
}

TerminalProfile*
terminal_app_get_profile_by_name (TerminalApp *app,
                                  const char *name)
{
	g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	return g_hash_table_lookup (app->profiles, name);
}

TerminalProfile*
terminal_app_get_profile_by_visible_name (TerminalApp *app,
        const char *name)
{
	LookupInfo info;

	g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	info.result = NULL;
	info.target = name;

	g_hash_table_foreach (app->profiles,
	                      profiles_lookup_by_visible_name_foreach,
	                      &info);
	return info.result;
}

TerminalProfile*
terminal_app_get_default_profile (TerminalApp *app)
{
	g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

	return app->default_profile;
}

TerminalProfile*
terminal_app_get_profile_for_new_term (TerminalApp *app)
{
	GHashTableIter iter;
	TerminalProfile *profile = NULL;
	TerminalProfile **profileptr = &profile;

	g_return_val_if_fail (TERMINAL_IS_APP (app), NULL);

	if (app->default_profile)
		return app->default_profile;

	g_hash_table_iter_init (&iter, app->profiles);
	if (g_hash_table_iter_next (&iter, NULL, (gpointer *) profileptr))
		return profile;

	return NULL;
}

GHashTable *
terminal_app_get_encodings (TerminalApp *app)
{
	return app->encodings;
}

/**
 * terminal_app_ensure_encoding:
 * @app:
 * @charset:
 *
 * Ensures there's a #TerminalEncoding for @charset available.
 */
TerminalEncoding *
terminal_app_ensure_encoding (TerminalApp *app,
                              const char *charset)
{
	TerminalEncoding *encoding;

	encoding = g_hash_table_lookup (app->encodings, charset);
	if (encoding == NULL)
	{
		encoding = terminal_encoding_new (charset,
		                                  _("User Defined"),
		                                  TRUE,
		                                  TRUE /* scary! */);
		g_hash_table_insert (app->encodings,
		                     (gpointer) terminal_encoding_get_id (encoding),
		                     encoding);
	}

	return encoding;
}

/**
 * terminal_app_get_active_encodings:
 *
 * Returns: a newly allocated list of newly referenced #TerminalEncoding objects.
 */
GSList*
terminal_app_get_active_encodings (TerminalApp *app)
{
	GSList *list = NULL;
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, app->encodings);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		TerminalEncoding *encoding = (TerminalEncoding *) value;

		if (!encoding->is_active)
			continue;

		list = g_slist_prepend (list, terminal_encoding_ref (encoding));
	}

	return g_slist_sort (list, (GCompareFunc) compare_encodings);
}

void
terminal_app_save_config (TerminalApp *app,
                          GKeyFile *key_file)
{
	GList *lw;
	guint n = 0;
	GPtrArray *window_names_array;
	char **window_names;
	gsize len;

	g_key_file_set_comment (key_file, NULL, NULL, "Written by " PACKAGE_STRING, NULL);

	g_key_file_set_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_VERSION, TERMINAL_CONFIG_VERSION);
	g_key_file_set_integer (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_COMPAT_VERSION, TERMINAL_CONFIG_COMPAT_VERSION);

	window_names_array = g_ptr_array_sized_new (g_list_length (app->windows) + 1);

	for (lw = app->windows; lw != NULL; lw = lw->next)
	{
		TerminalWindow *window = TERMINAL_WINDOW (lw->data);
		char *group;

		group = g_strdup_printf ("Window%u", n++);
		g_ptr_array_add (window_names_array, group);

		terminal_window_save_state (window, key_file, group);
	}

	len = window_names_array->len;
	g_ptr_array_add (window_names_array, NULL);
	window_names = (char **) g_ptr_array_free (window_names_array, FALSE);
	g_key_file_set_string_list (key_file, TERMINAL_CONFIG_GROUP, TERMINAL_CONFIG_PROP_WINDOWS, (const char * const *) window_names, len);
	g_strfreev (window_names);
}

gboolean
terminal_app_save_config_file (TerminalApp *app,
                               const char *file_name,
                               GError **error)
{
	GKeyFile *key_file;
	char *data;
	gsize len;
	gboolean result;

	key_file = g_key_file_new ();
	terminal_app_save_config (app, key_file);

	data = g_key_file_to_data (key_file, &len, NULL);
	result = g_file_set_contents (file_name, data, len, error);
	g_free (data);

	return result;
}
