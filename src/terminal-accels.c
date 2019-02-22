/*
 * Copyright © 2001, 2002 Havoc Pennington, Red Hat Inc.
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

#include <string.h>

#include <gdk/gdkkeysyms.h>

#include "terminal-accels.h"
#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-profile.h"
#include "terminal-util.h"

/* NOTES
 *
 * There are two sources of keybindings changes, from GSettings and from
 * the accel map (happens with in-place menu editing).
 *
 * When a keybinding GSettins key changes, we propagate that into the
 * accel map.
 * When the accel map changes, we queue a sync to GSettings.
 *
 * To avoid infinite loops, we short-circuit in both directions
 * if the value is unchanged from last known.
 *
 * In the keybinding editor, when editing or clearing an accel, we write
 * the change directly to GSettings and rely on the GSettings callback to
 * actually apply the change to the accel map.
 */

#define ACCEL_PATH_ROOT "<Actions>/Main/"
#define ACCEL_PATH_NEW_TAB              ACCEL_PATH_ROOT "FileNewTab"
#define ACCEL_PATH_NEW_WINDOW           ACCEL_PATH_ROOT "FileNewWindow"
#define ACCEL_PATH_NEW_PROFILE          ACCEL_PATH_ROOT "FileNewProfile"
#define ACCEL_PATH_SAVE_CONTENTS        ACCEL_PATH_ROOT "FileSaveContents"
#define ACCEL_PATH_CLOSE_TAB            ACCEL_PATH_ROOT "FileCloseTab"
#define ACCEL_PATH_CLOSE_WINDOW         ACCEL_PATH_ROOT "FileCloseWindow"
#define ACCEL_PATH_COPY                 ACCEL_PATH_ROOT "EditCopy"
#define ACCEL_PATH_PASTE                ACCEL_PATH_ROOT "EditPaste"
#define ACCEL_PATH_SELECT_ALL           ACCEL_PATH_ROOT "EditSelectAll"
#define ACCEL_PATH_SEARCH_FIND          ACCEL_PATH_ROOT "SearchFind"
#define ACCEL_PATH_SEARCH_FIND_NEXT     ACCEL_PATH_ROOT "SearchFindNext"
#define ACCEL_PATH_SEARCH_FIND_PREVIOUS ACCEL_PATH_ROOT "SearchFindPrevious"
#define ACCEL_PATH_TOGGLE_MENUBAR       ACCEL_PATH_ROOT "ViewMenubar"
#define ACCEL_PATH_FULL_SCREEN          ACCEL_PATH_ROOT "ViewFullscreen"
#define ACCEL_PATH_RESET                ACCEL_PATH_ROOT "TerminalReset"
#define ACCEL_PATH_RESET_AND_CLEAR      ACCEL_PATH_ROOT "TerminalResetClear"
#define ACCEL_PATH_PREV_PROFILE         ACCEL_PATH_ROOT "ProfilePrevious"
#define ACCEL_PATH_NEXT_PROFILE         ACCEL_PATH_ROOT "ProfileNext"
#define ACCEL_PATH_PREV_TAB             ACCEL_PATH_ROOT "TabsPrevious"
#define ACCEL_PATH_NEXT_TAB             ACCEL_PATH_ROOT "TabsNext"
#define ACCEL_PATH_SET_TERMINAL_TITLE   ACCEL_PATH_ROOT "TerminalSetTitle"
#define ACCEL_PATH_HELP                 ACCEL_PATH_ROOT "HelpContents"
#define ACCEL_PATH_ZOOM_IN              ACCEL_PATH_ROOT "ViewZoomIn"
#define ACCEL_PATH_ZOOM_OUT             ACCEL_PATH_ROOT "ViewZoomOut"
#define ACCEL_PATH_ZOOM_NORMAL          ACCEL_PATH_ROOT "ViewZoom100"
#define ACCEL_PATH_MOVE_TAB_LEFT        ACCEL_PATH_ROOT "TabsMoveLeft"
#define ACCEL_PATH_MOVE_TAB_RIGHT       ACCEL_PATH_ROOT "TabsMoveRight"
#define ACCEL_PATH_DETACH_TAB           ACCEL_PATH_ROOT "TabsDetach"
#define ACCEL_PATH_SWITCH_TAB_PREFIX    ACCEL_PATH_ROOT "TabsSwitch"

#define KEY_CLOSE_TAB            "close-tab"
#define KEY_CLOSE_WINDOW         "close-window"
#define KEY_COPY                 "copy"
#define KEY_DETACH_TAB           "detach-tab"
#define KEY_FULL_SCREEN          "full-screen"
#define KEY_HELP                 "help"
#define KEY_MOVE_TAB_LEFT        "move-tab-left"
#define KEY_MOVE_TAB_RIGHT       "move-tab-right"
#define KEY_NEW_PROFILE          "new-profile"
#define KEY_NEW_TAB              "new-tab"
#define KEY_NEW_WINDOW           "new-window"
#define KEY_NEXT_PROFILE         "next-profile"
#define KEY_NEXT_TAB             "next-tab"
#define KEY_PASTE                "paste"
#define KEY_PREV_PROFILE         "prev-profile"
#define KEY_PREV_TAB             "prev-tab"
#define KEY_RESET_AND_CLEAR      "reset-and-clear"
#define KEY_RESET                "reset"
#define KEY_SEARCH_FIND          "search-find"
#define KEY_SEARCH_FIND_NEXT     "search-find-next"
#define KEY_SEARCH_FIND_PREVIOUS "search-find-previous"
#define KEY_SELECT_ALL           "select-all"
#define KEY_SAVE_CONTENTS        "save-contents"
#define KEY_SET_TERMINAL_TITLE   "set-terminal-title"
#define KEY_TOGGLE_MENUBAR       "toggle-menubar"
#define KEY_ZOOM_IN              "zoom-in"
#define KEY_ZOOM_NORMAL          "zoom-normal"
#define KEY_ZOOM_OUT             "zoom-out"
#define KEY_SWITCH_TAB_PREFIX    "switch-to-tab-"

#if 1
/*
* We don't want to enable content saving until vte supports it async.
* So we disable this code for stable versions.
*/
#include "terminal-version.h"

#if (TERMINAL_MINOR_VERSION & 1) != 0
#define ENABLE_SAVE
#else
#undef ENABLE_SAVE
#endif
#endif

typedef struct
{
	const char *user_visible_name;
	const char *gsettings_key;
	const char *accel_path;
	/* last values received from GSettings */
	GdkModifierType gsettings_mask;
	guint gsettings_keyval;
	GClosure *closure;
	/* have gotten a notification from gtk */
	gboolean needs_gsettings_sync;
	gboolean accel_path_unlocked;
} KeyEntry;

typedef struct
{
	KeyEntry *key_entry;
	guint n_elements;
	const char *user_visible_name;
} KeyEntryList;

static KeyEntry file_entries[] =
{
	{
		N_("New Tab"),
		KEY_NEW_TAB, ACCEL_PATH_NEW_TAB, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_t, NULL, FALSE, TRUE
	},
	{
		N_("New Window"),
		KEY_NEW_WINDOW, ACCEL_PATH_NEW_WINDOW, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_n, NULL, FALSE, TRUE
	},
	{
		N_("New Profile"),
		KEY_NEW_PROFILE, ACCEL_PATH_NEW_PROFILE, 0, 0, NULL, FALSE, TRUE
	},
#ifdef ENABLE_SAVE
	{
		N_("Save Contents"),
		KEY_SAVE_CONTENTS, ACCEL_PATH_SAVE_CONTENTS, 0, 0, NULL, FALSE, TRUE
	},
#endif
	{
		N_("Close Tab"),
		KEY_CLOSE_TAB, ACCEL_PATH_CLOSE_TAB, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_w, NULL, FALSE, TRUE
	},
	{
		N_("Close Window"),
		KEY_CLOSE_WINDOW, ACCEL_PATH_CLOSE_WINDOW, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_q, NULL, FALSE, TRUE
	},
};

static KeyEntry edit_entries[] =
{
	{
		N_("Copy"),
		KEY_COPY, ACCEL_PATH_COPY, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_c, NULL, FALSE, TRUE
	},
	{
		N_("Paste"),
		KEY_PASTE, ACCEL_PATH_PASTE, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_v, NULL, FALSE, TRUE
	},
	{
		N_("Select All"),
		KEY_SELECT_ALL, ACCEL_PATH_SELECT_ALL, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_a, NULL, FALSE, TRUE
	}
};

static KeyEntry view_entries[] =
{
	{
		N_("Hide and Show menubar"),
		KEY_TOGGLE_MENUBAR, ACCEL_PATH_TOGGLE_MENUBAR, 0, 0, NULL, FALSE, TRUE
	},
	{
		N_("Full Screen"),
		KEY_FULL_SCREEN, ACCEL_PATH_FULL_SCREEN, 0, GDK_KEY_F11, NULL, FALSE, TRUE
	},
	{
		N_("Zoom In"),
		KEY_ZOOM_IN, ACCEL_PATH_ZOOM_IN, GDK_CONTROL_MASK, GDK_KEY_plus, NULL, FALSE, TRUE
	},
	{
		N_("Zoom Out"),
		KEY_ZOOM_OUT, ACCEL_PATH_ZOOM_OUT, GDK_CONTROL_MASK, GDK_KEY_minus, NULL, FALSE, TRUE
	},
	{
		N_("Normal Size"),
		KEY_ZOOM_NORMAL, ACCEL_PATH_ZOOM_NORMAL, GDK_CONTROL_MASK, GDK_KEY_0, NULL, FALSE, TRUE
	}
};

static KeyEntry search_entries[] =
{
	{
		N_("Find"),
		KEY_SEARCH_FIND, ACCEL_PATH_SEARCH_FIND, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_f, NULL, FALSE, TRUE
	},
	{
		N_("Find Next"),
		KEY_SEARCH_FIND_NEXT, ACCEL_PATH_SEARCH_FIND_NEXT, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_h, NULL, FALSE, TRUE
	},
	{
		N_("Find Previous"),
		KEY_SEARCH_FIND_PREVIOUS, ACCEL_PATH_SEARCH_FIND_PREVIOUS, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_g, NULL, FALSE, TRUE
	}
};

static KeyEntry terminal_entries[] =
{
	{
		N_("Set Title"),
		KEY_SET_TERMINAL_TITLE, ACCEL_PATH_SET_TERMINAL_TITLE, 0, 0, NULL, FALSE, TRUE
	},
	{
		N_("Reset"),
		KEY_RESET, ACCEL_PATH_RESET, 0, 0, NULL, FALSE, TRUE
	},
	{
		N_("Reset and Clear"),
		KEY_RESET_AND_CLEAR, ACCEL_PATH_RESET_AND_CLEAR, 0, 0, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Previous Profile"),
		KEY_PREV_PROFILE, ACCEL_PATH_PREV_PROFILE, GDK_MOD1_MASK, GDK_KEY_Page_Up, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Next Profile"),
		KEY_NEXT_PROFILE, ACCEL_PATH_NEXT_PROFILE, GDK_MOD1_MASK, GDK_KEY_Page_Down, NULL, FALSE, TRUE
	},
};

static KeyEntry tabs_entries[] =
{
	{
		N_("Switch to Previous Tab"),
		KEY_PREV_TAB, ACCEL_PATH_PREV_TAB, GDK_CONTROL_MASK, GDK_KEY_Page_Up, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Next Tab"),
		KEY_NEXT_TAB, ACCEL_PATH_NEXT_TAB, GDK_CONTROL_MASK, GDK_KEY_Page_Down, NULL, FALSE, TRUE
	},
	{
		N_("Move Tab to the Left"),
		KEY_MOVE_TAB_LEFT, ACCEL_PATH_MOVE_TAB_LEFT, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_Page_Up, NULL, FALSE, TRUE
	},
	{
		N_("Move Tab to the Right"),
		KEY_MOVE_TAB_RIGHT, ACCEL_PATH_MOVE_TAB_RIGHT, GDK_SHIFT_MASK | GDK_CONTROL_MASK, GDK_KEY_Page_Down, NULL, FALSE, TRUE
	},
	{
		N_("Detach Tab"),
		KEY_DETACH_TAB, ACCEL_PATH_DETACH_TAB, 0, 0, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 1"),
		KEY_SWITCH_TAB_PREFIX "1",
		ACCEL_PATH_SWITCH_TAB_PREFIX "1", GDK_MOD1_MASK, GDK_KEY_1, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 2"),
		KEY_SWITCH_TAB_PREFIX "2",
		ACCEL_PATH_SWITCH_TAB_PREFIX "2", GDK_MOD1_MASK, GDK_KEY_2, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 3"),
		KEY_SWITCH_TAB_PREFIX "3",
		ACCEL_PATH_SWITCH_TAB_PREFIX "3", GDK_MOD1_MASK, GDK_KEY_3, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 4"),
		KEY_SWITCH_TAB_PREFIX "4",
		ACCEL_PATH_SWITCH_TAB_PREFIX "4", GDK_MOD1_MASK, GDK_KEY_4, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 5"),
		KEY_SWITCH_TAB_PREFIX "5",
		ACCEL_PATH_SWITCH_TAB_PREFIX "5", GDK_MOD1_MASK, GDK_KEY_5, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 6"),
		KEY_SWITCH_TAB_PREFIX "6",
		ACCEL_PATH_SWITCH_TAB_PREFIX "6", GDK_MOD1_MASK, GDK_KEY_6, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 7"),
		KEY_SWITCH_TAB_PREFIX "7",
		ACCEL_PATH_SWITCH_TAB_PREFIX "7", GDK_MOD1_MASK, GDK_KEY_7, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 8"),
		KEY_SWITCH_TAB_PREFIX "8",
		ACCEL_PATH_SWITCH_TAB_PREFIX "8", GDK_MOD1_MASK, GDK_KEY_8, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 9"),
		KEY_SWITCH_TAB_PREFIX "9",
		ACCEL_PATH_SWITCH_TAB_PREFIX "9", GDK_MOD1_MASK, GDK_KEY_9, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 10"),
		KEY_SWITCH_TAB_PREFIX "10",
		ACCEL_PATH_SWITCH_TAB_PREFIX "10", GDK_MOD1_MASK, GDK_KEY_0, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 11"),
		KEY_SWITCH_TAB_PREFIX "11",
		ACCEL_PATH_SWITCH_TAB_PREFIX "11", 0, 0, NULL, FALSE, TRUE
	},
	{
		N_("Switch to Tab 12"),
		KEY_SWITCH_TAB_PREFIX "12",
		ACCEL_PATH_SWITCH_TAB_PREFIX "12", 0, 0, NULL, FALSE, TRUE
	}
};

static KeyEntry help_entries[] =
{
	{ N_("Contents"), KEY_HELP, ACCEL_PATH_HELP, 0, GDK_KEY_F1, NULL, FALSE, TRUE }
};

static KeyEntryList all_entries[] =
{
	{ file_entries, G_N_ELEMENTS (file_entries), N_("File") },
	{ edit_entries, G_N_ELEMENTS (edit_entries), N_("Edit") },
	{ view_entries, G_N_ELEMENTS (view_entries), N_("View") },
	{ search_entries, G_N_ELEMENTS (search_entries), N_("Search") },
	{ terminal_entries, G_N_ELEMENTS (terminal_entries), N_("Terminal") },
	{ tabs_entries, G_N_ELEMENTS (tabs_entries), N_("Tabs") },
	{ help_entries, G_N_ELEMENTS (help_entries), N_("Help") }
};

enum
{
    ACTION_COLUMN,
    KEYVAL_COLUMN,
    N_COLUMNS
};

static void keys_change_notify (GSettings *settings,
                                const gchar *key,
                                gpointer user_data);

static void accel_changed_callback (GtkAccelGroup  *accel_group,
                                    guint           keyval,
                                    GdkModifierType modifier,
                                    GClosure       *accel_closure,
                                    gpointer        data);

static gboolean binding_from_string (const char      *str,
                                     guint           *accelerator_key,
                                     GdkModifierType *accelerator_mods);

static gboolean binding_from_value  (GVariant        *value,
                                     guint           *accelerator_key,
                                     GdkModifierType *accelerator_mods);

static gboolean sync_idle_cb (gpointer data);

static guint sync_idle_id = 0;
static GtkAccelGroup *notification_group = NULL;
/* never set GSettings keys in response to receiving a GSettings notify. */
static int inside_gsettings_notify = 0;
static GtkWidget *edit_keys_dialog = NULL;
static GtkTreeStore *edit_keys_store = NULL;
static GHashTable *gsettings_key_to_entry;
static GSettings *settings_keybindings;

static char*
binding_name (guint            keyval,
              GdkModifierType  mask)
{
	if (keyval != 0)
		return gtk_accelerator_name (keyval, mask);

	return g_strdup ("disabled");
}

static char*
binding_display_name (guint            keyval,
                      GdkModifierType  mask)
{
	if (keyval != 0)
		return gtk_accelerator_get_label (keyval, mask);

	return g_strdup (_("Disabled"));
}

void
terminal_accels_init (void)
{
	guint i, j;

	settings_keybindings = g_settings_new (CONF_KEYS_SCHEMA);

	g_signal_connect (settings_keybindings,
			  "changed",
			  G_CALLBACK(keys_change_notify),
			  NULL);

	gsettings_key_to_entry = g_hash_table_new (g_str_hash, g_str_equal);

	notification_group = gtk_accel_group_new ();

	for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
	{
		for (j = 0; j < all_entries[i].n_elements; ++j)
		{
			KeyEntry *key_entry;

			key_entry = &(all_entries[i].key_entry[j]);

			g_hash_table_insert (gsettings_key_to_entry,
			                     (gpointer) key_entry->gsettings_key,
			                     key_entry);

			key_entry->closure = g_closure_new_simple (sizeof (GClosure), key_entry);

			g_closure_ref (key_entry->closure);
			g_closure_sink (key_entry->closure);

			gtk_accel_group_connect_by_path (notification_group,
			                                 I_(key_entry->accel_path),
			                                 key_entry->closure);
			keys_change_notify (settings_keybindings, key_entry->gsettings_key, NULL);
		}
	}

	g_signal_connect (notification_group, "accel-changed",
	                  G_CALLBACK (accel_changed_callback), NULL);
}

void
terminal_accels_shutdown (void)
{

	g_signal_handlers_disconnect_by_func (settings_keybindings,
					      G_CALLBACK(keys_change_notify),
					      NULL);
	g_object_unref (settings_keybindings);

	if (sync_idle_id != 0)
	{
		g_source_remove (sync_idle_id);
		sync_idle_id = 0;

		sync_idle_cb (NULL);
	}

	g_hash_table_destroy (gsettings_key_to_entry);
	gsettings_key_to_entry = NULL;

	g_object_unref (notification_group);
	notification_group = NULL;
}

static gboolean
update_model_foreach (GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer      data)
{
	KeyEntry *key_entry = NULL;

	gtk_tree_model_get (model, iter,
	                    KEYVAL_COLUMN, &key_entry,
	                    -1);

	if (key_entry == (KeyEntry *) data)
	{
		gtk_tree_model_row_changed (model, path, iter);
		return TRUE;
	}
	return FALSE;
}

static void
keys_change_notify (GSettings *settings,
                    const gchar *key,
                    gpointer user_data)
{
	GVariant *val;
	KeyEntry *key_entry;
	GdkModifierType mask;
	guint keyval;

	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "key %s changed\n",
	                       key);

	val = g_settings_get_value (settings, key);

#ifdef MATE_ENABLE_DEBUG
	_TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
	{
		if (val == NULL)
			_terminal_debug_print (TERMINAL_DEBUG_ACCELS, " changed to be unset\n");
		else if (!g_variant_is_of_type (val, G_VARIANT_TYPE_STRING))
			_terminal_debug_print (TERMINAL_DEBUG_ACCELS, " changed to non-string value\n");
		else
			_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
			                       " changed to \"%s\"\n",
			                       g_variant_get_string (val, NULL));
	}
#endif

	key_entry = g_hash_table_lookup (gsettings_key_to_entry, key);
	if (!key_entry)
	{
		/* shouldn't really happen, but let's be safe */
		_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
		                       "  WARNING: KeyEntry for changed key not found, bailing out\n");
		return;
	}

	if (!binding_from_value (val, &keyval, &mask))
	{
		const char *str = g_variant_is_of_type (val, G_VARIANT_TYPE_STRING) ? g_variant_get_string (val, NULL) : NULL;
		g_printerr ("The value \"%s\" of configuration key %s is not a valid accelerator\n",
		            str ? str : "(null)",
		            key_entry->gsettings_key);
		return;
	}
	key_entry->gsettings_keyval = keyval;
	key_entry->gsettings_mask = mask;

	/* Unlock the path, so we can change its accel */
	if (!key_entry->accel_path_unlocked)
		gtk_accel_map_unlock_path (key_entry->accel_path);

	/* sync over to GTK */
	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "changing path %s to %s\n",
	                       key_entry->accel_path,
	                       binding_name (keyval, mask)); /* memleak */
	inside_gsettings_notify += 1;
	/* Note that this may return FALSE, e.g. when the entry was already set correctly. */
	gtk_accel_map_change_entry (key_entry->accel_path,
	                            keyval, mask,
	                            TRUE);
	inside_gsettings_notify -= 1;

	/* Lock the path if the GSettings key isn't writable */
	key_entry->accel_path_unlocked = g_settings_is_writable (settings, key);
	if (!key_entry->accel_path_unlocked)
		gtk_accel_map_lock_path (key_entry->accel_path);

	/* This seems necessary to update the tree model, since sometimes the
	 * notification on the notification_group seems not to be emitted correctly.
	 * Without this change, when trying to set an accel to e.g. Alt-T (while the main
	 * menu in the terminal windows is _Terminal with Alt-T mnemonic) only displays
	 * the accel change after a re-expose of the row.
	 * FIXME: Find out *why* the accel-changed signal is wrong here!
	 */
	if (edit_keys_store)
		gtk_tree_model_foreach (GTK_TREE_MODEL (edit_keys_store), update_model_foreach, key_entry);

	g_variant_unref(val);
}

static void
accel_changed_callback (GtkAccelGroup  *accel_group,
                        guint           keyval,
                        GdkModifierType modifier,
                        GClosure       *accel_closure,
                        gpointer        data)
{
	/* FIXME because GTK accel API is so nonsensical, we get
	 * a notify for each closure, on both the added and the removed
	 * accelerator. We just use the accel closure to find our
	 * accel entry, then update the value of that entry.
	 * We use an idle function to avoid setting the entry
	 * in GSettings when the accelerator gets removed and then
	 * setting it again when it gets added.
	 */
	KeyEntry *key_entry;

	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "Changed accel %s closure %p\n",
	                       binding_name (keyval, modifier), /* memleak */
	                       accel_closure);

	if (inside_gsettings_notify)
	{
		_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
		                       "Ignoring change from gtk because we're inside a GSettings notify\n");
		return;
	}

	key_entry = accel_closure->data;
	g_assert (key_entry);

	key_entry->needs_gsettings_sync = TRUE;

	if (sync_idle_id == 0)
		sync_idle_id = g_idle_add (sync_idle_cb, NULL);
}

static gboolean
binding_from_string (const char      *str,
                     guint           *accelerator_key,
                     GdkModifierType *accelerator_mods)
{
	if (str == NULL ||
	        strcmp (str, "disabled") == 0)
	{
		*accelerator_key = 0;
		*accelerator_mods = 0;
		return TRUE;
	}

	gtk_accelerator_parse (str, accelerator_key, accelerator_mods);
	if (*accelerator_key == 0 &&
	        *accelerator_mods == 0)
		return FALSE;

	return TRUE;
}

static gboolean
binding_from_value (GVariant         *value,
                    guint            *accelerator_key,
                    GdkModifierType  *accelerator_mods)
{
	if (value == NULL)
	{
		/* unset */
		*accelerator_key = 0;
		*accelerator_mods = 0;
		return TRUE;
	}

	if (!g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
		return FALSE;

	return binding_from_string (g_variant_get_string (value,NULL),
	                            accelerator_key,
	                            accelerator_mods);
}

static void
add_key_entry_to_changeset (gpointer key,
                            KeyEntry *key_entry,
                            GSettings *changeset)
{
	GtkAccelKey gtk_key;

	if (!key_entry->needs_gsettings_sync)
		return;

	key_entry->needs_gsettings_sync = FALSE;

	if (gtk_accel_map_lookup_entry (key_entry->accel_path, &gtk_key) &&
	        (gtk_key.accel_key != key_entry->gsettings_keyval ||
	         gtk_key.accel_mods != key_entry->gsettings_mask))
	{
		char *accel_name;

		accel_name = binding_name (gtk_key.accel_key, gtk_key.accel_mods);
		g_settings_set_string (changeset, key_entry->gsettings_key, accel_name);
		g_free (accel_name);
	}
}

static gboolean
sync_idle_cb (gpointer data)
{
	GSettings *changeset;

	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "GSettings sync handler\n");

	sync_idle_id = 0;

	changeset = g_settings_new (CONF_KEYS_SCHEMA);
	g_settings_delay (changeset);

	g_hash_table_foreach (gsettings_key_to_entry, (GHFunc) add_key_entry_to_changeset, changeset);
	g_settings_apply(changeset);

	g_object_unref (changeset);

	return FALSE;
}

/* We have the same KeyEntry* in both columns;
 * we only have two columns because we want to be able
 * to sort by either one of them.
 */

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
	KeyEntry *ke;

	gtk_tree_model_get (model, iter,
	                    KEYVAL_COLUMN, &ke,
	                    -1);

	if (ke == NULL)
		/* This is a title row */
		g_object_set (cell,
		              "visible", FALSE,
		              NULL);
	else
		g_object_set (cell,
		              "visible", TRUE,
		              "sensitive", ke->accel_path_unlocked,
		              "editable", ke->accel_path_unlocked,
		              "accel-key", ke->gsettings_keyval,
		              "accel-mods", ke->gsettings_mask,
		              NULL);
}

static int
accel_compare_func (GtkTreeModel *model,
                    GtkTreeIter  *a,
                    GtkTreeIter  *b,
                    gpointer      user_data)
{
	KeyEntry *ke_a;
	KeyEntry *ke_b;
	char *name_a;
	char *name_b;
	int result;

	gtk_tree_model_get (model, a,
	                    KEYVAL_COLUMN, &ke_a,
	                    -1);
	if (ke_a == NULL)
	{
		gtk_tree_model_get (model, a,
		                    ACTION_COLUMN, &name_a,
		                    -1);
	}
	else
	{
		name_a = binding_display_name (ke_a->gsettings_keyval,
		                               ke_a->gsettings_mask);
	}

	gtk_tree_model_get (model, b,
	                    KEYVAL_COLUMN, &ke_b,
	                    -1);
	if (ke_b == NULL)
	{
		gtk_tree_model_get (model, b,
		                    ACTION_COLUMN, &name_b,
		                    -1);
	}
	else
	{
		name_b = binding_display_name (ke_b->gsettings_keyval,
		                               ke_b->gsettings_mask);
	}

	result = g_utf8_collate (name_a, name_b);

	g_free (name_a);
	g_free (name_b);

	return result;
}

static void
treeview_accel_changed_cb (GtkAccelGroup  *accel_group,
                           guint keyval,
                           GdkModifierType modifier,
                           GClosure *accel_closure,
                           GtkTreeModel *model)
{
	gtk_tree_model_foreach (model, update_model_foreach, accel_closure->data);
}

static void
accel_edited_callback (GtkCellRendererAccel *cell,
                       gchar                *path_string,
                       guint                 keyval,
                       GdkModifierType       mask,
                       guint                 hardware_keycode,
                       GtkTreeView          *view)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	KeyEntry *ke;
	GtkAccelGroupEntry *entries;
	guint n_entries;
	char *str;

	model = gtk_tree_view_get_model (view);

	path = gtk_tree_path_new_from_string (path_string);
	if (!path)
		return;

	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, KEYVAL_COLUMN, &ke, -1);

	/* sanity check */
	if (ke == NULL)
		return;

	/* Check if we already have an entry using this accel */
	entries = gtk_accel_group_query (notification_group, keyval, mask, &n_entries);
	if (n_entries > 0)
	{
		if (entries[0].accel_path_quark != g_quark_from_string (ke->accel_path))
		{
			GtkWidget *dialog;
			char *name;
			KeyEntry *other_key;

			name = gtk_accelerator_get_label (keyval, mask);
			other_key = entries[0].closure->data;
			g_assert (other_key);

			dialog =
			    gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
			                            GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
			                            GTK_MESSAGE_WARNING,
			                            GTK_BUTTONS_OK,
			                            _("The shortcut key “%s” is already bound to the “%s” action"),
			                            name,

other_key->user_visible_name ? _(other_key->user_visible_name) : other_key->gsettings_key);
			g_free (name);

			g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_window_present (GTK_WINDOW (dialog));
		}

		return;
	}

	str = binding_name (keyval, mask);

	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "Edited path %s keyval %s, setting GSettings to %s\n",
	                       ke->accel_path,
	                       gdk_keyval_name (keyval) ? gdk_keyval_name (keyval) : "null",
	                       str);
#ifdef MATE_ENABLE_DEBUG
	_TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
	{
		GtkAccelKey old_key;

		if (gtk_accel_map_lookup_entry (ke->accel_path, &old_key))
		{
			_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
			                       "  Old entry of path %s is keyval %s mask %x\n",
			                       ke->accel_path, gdk_keyval_name (old_key.accel_key), old_key.accel_mods);
		}
		else
		{
			_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
			                       "  Failed to look up the old entry of path %s\n",
			                       ke->accel_path);
		}
	}
#endif

	g_settings_set_string (settings_keybindings,
	                       ke->gsettings_key,
	                       str);
	g_free (str);
}

static void
accel_cleared_callback (GtkCellRendererAccel *cell,
                        gchar                *path_string,
                        GtkTreeView          *view)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	KeyEntry *ke;
	char *str;

	model = gtk_tree_view_get_model (view);

	path = gtk_tree_path_new_from_string (path_string);
	if (!path)
		return;

	if (!gtk_tree_model_get_iter (model, &iter, path))
	{
		gtk_tree_path_free (path);
		return;
	}
	gtk_tree_path_free (path);

	gtk_tree_model_get (model, &iter, KEYVAL_COLUMN, &ke, -1);

	/* sanity check */
	if (ke == NULL)
		return;

	ke->gsettings_keyval = 0;
	ke->gsettings_mask = 0;
	ke->needs_gsettings_sync = TRUE;

	str = binding_name (0, 0);

	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "Cleared keybinding for GSettings %s",
	                       ke->gsettings_key);

	g_settings_set_string (settings_keybindings,
	                       ke->gsettings_key,
	                       str);
	g_free (str);
}

static void
edit_keys_dialog_destroy_cb (GtkWidget *widget,
                             gpointer user_data)
{
	g_signal_handlers_disconnect_by_func (notification_group, G_CALLBACK (treeview_accel_changed_cb), user_data);
	edit_keys_dialog = NULL;
	edit_keys_store = NULL;
}

static void
edit_keys_dialog_response_cb (GtkWidget *editor,
                              int response,
                              gpointer use_data)
{
	if (response == GTK_RESPONSE_HELP)
	{
		terminal_util_show_help ("mate-terminal-shortcuts", GTK_WINDOW (editor));
		return;
	}

	gtk_widget_destroy (editor);
}

#ifdef MATE_ENABLE_DEBUG
static void
row_changed (GtkTreeModel *tree_model,
             GtkTreePath  *path,
             GtkTreeIter  *iter,
             gpointer      user_data)
{
	_terminal_debug_print (TERMINAL_DEBUG_ACCELS,
	                       "ROW-CHANGED [%s]\n", gtk_tree_path_to_string (path) /* leak */);
}
#endif

void
terminal_edit_keys_dialog_show (GtkWindow *transient_parent)
{
	TerminalApp *app;
	GtkWidget *dialog, *tree_view, *disable_mnemonics_button, *disable_menu_accel_button;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;
	GtkTreeStore *tree;
	guint i;

	if (edit_keys_dialog != NULL)
		goto done;

	if (!terminal_util_load_builder_resource (TERMINAL_RESOURCES_PATH_PREFIX G_DIR_SEPARATOR_S "ui/keybinding-editor.ui",
	                                      "keybindings-dialog", &dialog,
	                                      "disable-mnemonics-checkbutton", &disable_mnemonics_button,
	                                      "disable-menu-accel-checkbutton", &disable_menu_accel_button,
	                                      "accelerators-treeview", &tree_view,
	                                      NULL))
		return;

	app = terminal_app_get ();
	terminal_util_bind_object_property_to_widget (G_OBJECT (app), TERMINAL_APP_ENABLE_MNEMONICS,
	        disable_mnemonics_button, 0);
	terminal_util_bind_object_property_to_widget (G_OBJECT (app), TERMINAL_APP_ENABLE_MENU_BAR_ACCEL,
	        disable_menu_accel_button, 0);

	/* Column 1 */
	cell_renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("_Action"),
	         cell_renderer,
	         "text", ACTION_COLUMN,
	         NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
	gtk_tree_view_column_set_sort_column_id (column, ACTION_COLUMN);

	/* Column 2 */
	cell_renderer = gtk_cell_renderer_accel_new ();
	g_object_set (cell_renderer,
	              "editable", TRUE,
	              "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK,
	              NULL);
	g_signal_connect (cell_renderer, "accel-edited",
	                  G_CALLBACK (accel_edited_callback), tree_view);
	g_signal_connect (cell_renderer, "accel-cleared",
	                  G_CALLBACK (accel_cleared_callback), tree_view);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Shortcut _Key"));
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, cell_renderer, accel_set_func, NULL, NULL);
	gtk_tree_view_column_set_sort_column_id (column, KEYVAL_COLUMN);
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);

	/* Add the data */

	tree = edit_keys_store = gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

#ifdef MATE_ENABLE_DEBUG
	_TERMINAL_DEBUG_IF (TERMINAL_DEBUG_ACCELS)
	g_signal_connect (tree, "row-changed", G_CALLBACK (row_changed), NULL);
#endif

	for (i = 0; i < G_N_ELEMENTS (all_entries); ++i)
	{
		GtkTreeIter parent_iter;
		guint j;

		gtk_tree_store_append (tree, &parent_iter, NULL);
		gtk_tree_store_set (tree, &parent_iter,
		                    ACTION_COLUMN, _(all_entries[i].user_visible_name),
		                    -1);

		for (j = 0; j < all_entries[i].n_elements; ++j)
		{
			KeyEntry *key_entry = &(all_entries[i].key_entry[j]);
			GtkTreeIter iter;

			gtk_tree_store_insert_with_values (tree, &iter, &parent_iter, -1,
			                                   ACTION_COLUMN, _(key_entry->user_visible_name),
			                                   KEYVAL_COLUMN, key_entry,
			                                   -1);
		}
	}

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (tree),
	                                 KEYVAL_COLUMN, accel_compare_func,
	                                 NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (tree), ACTION_COLUMN,
	                                      GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), GTK_TREE_MODEL (tree));
	g_object_unref (tree);

	gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

	g_signal_connect (notification_group, "accel-changed",
	                  G_CALLBACK (treeview_accel_changed_cb), tree);

	edit_keys_dialog = dialog;
	g_signal_connect (dialog, "destroy",
	                  G_CALLBACK (edit_keys_dialog_destroy_cb), tree);
	g_signal_connect (dialog, "response",
	                  G_CALLBACK (edit_keys_dialog_response_cb),
	                  NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 350);

done:
	gtk_window_set_transient_for (GTK_WINDOW (edit_keys_dialog), transient_parent);
	gtk_window_present (GTK_WINDOW (edit_keys_dialog));
}
