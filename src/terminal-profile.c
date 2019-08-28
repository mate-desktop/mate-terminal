/*
 * Copyright © 2001 Havoc Pennington
 * Copyright © 2002 Mathias Hasselmann
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
#include <stdlib.h>

#include <gtk/gtk.h>

#include "terminal-app.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "terminal-profile.h"
#include "terminal-screen.h"
#include "terminal-type-builtins.h"

/* To add a new key, you need to:
 *
 *  - add an entry to the enum below
 *  - add a #define with its name in terminal-profile.h
 *  - add a gobject property for it in terminal_profile_class_init
 *  - if the property's type needs special casing, add that to
 *    terminal_profile_gsettings_notify_cb and
 *    terminal_profile_gsettings_changeset_add
 *  - if necessary the default value cannot be handled via the paramspec,
 *    handle that in terminal_profile_reset_property_internal
 */
enum
{
    PROP_0,
    PROP_ALLOW_BOLD,
    PROP_BACKGROUND_COLOR,
    PROP_BACKGROUND_DARKNESS,
    PROP_BACKGROUND_IMAGE,
    PROP_BACKGROUND_IMAGE_FILE,
    PROP_BACKGROUND_TYPE,
    PROP_BACKSPACE_BINDING,
    PROP_BOLD_COLOR,
    PROP_BOLD_COLOR_SAME_AS_FG,
    PROP_CURSOR_BLINK_MODE,
    PROP_CURSOR_SHAPE,
    PROP_CUSTOM_COMMAND,
    PROP_DEFAULT_SIZE_COLUMNS,
    PROP_DEFAULT_SIZE_ROWS,
    PROP_DEFAULT_SHOW_MENUBAR,
    PROP_DELETE_BINDING,
    PROP_EXIT_ACTION,
    PROP_FONT,
    PROP_FOREGROUND_COLOR,
    PROP_LOGIN_SHELL,
    PROP_NAME,
    PROP_PALETTE,
    PROP_SCROLL_BACKGROUND,
    PROP_SCROLLBACK_LINES,
    PROP_SCROLLBACK_UNLIMITED,
    PROP_SCROLLBAR_POSITION,
    PROP_SCROLL_ON_KEYSTROKE,
    PROP_SCROLL_ON_OUTPUT,
    PROP_SILENT_BELL,
    PROP_TITLE,
    PROP_TITLE_MODE,
    PROP_USE_CUSTOM_COMMAND,
    PROP_USE_CUSTOM_DEFAULT_SIZE,
    PROP_USE_SKEY,
    PROP_USE_URLS,
    PROP_USE_SYSTEM_FONT,
    PROP_USE_THEME_COLORS,
    PROP_VISIBLE_NAME,
    PROP_WORD_CHARS,
    PROP_COPY_SELECTION,
    LAST_PROP
};

#define KEY_ALLOW_BOLD "allow-bold"
#define KEY_BACKGROUND_COLOR "background-color"
#define KEY_BACKGROUND_DARKNESS "background-darkness"
#define KEY_BACKGROUND_IMAGE_FILE "background-image"
#define KEY_BACKGROUND_TYPE "background-type"
#define KEY_BACKSPACE_BINDING "backspace-binding"
#define KEY_BOLD_COLOR "bold-color"
#define KEY_BOLD_COLOR_SAME_AS_FG "bold-color-same-as-fg"
#define KEY_CURSOR_BLINK_MODE "cursor-blink-mode"
#define KEY_CURSOR_SHAPE "cursor-shape"
#define KEY_CUSTOM_COMMAND "custom-command"
#define KEY_DEFAULT_SHOW_MENUBAR "default-show-menubar"
#define KEY_DEFAULT_SIZE_COLUMNS "default-size-columns"
#define KEY_DEFAULT_SIZE_ROWS "default-size-rows"
#define KEY_DELETE_BINDING "delete-binding"
#define KEY_EXIT_ACTION "exit-action"
#define KEY_FONT "font"
#define KEY_FOREGROUND_COLOR "foreground-color"
#define KEY_LOGIN_SHELL "login-shell"
#define KEY_PALETTE "palette"
#define KEY_SCROLL_BACKGROUND "scroll-background"
#define KEY_SCROLLBACK_LINES "scrollback-lines"
#define KEY_SCROLLBACK_UNLIMITED "scrollback-unlimited"
#define KEY_SCROLLBAR_POSITION "scrollbar-position"
#define KEY_SCROLL_ON_KEYSTROKE "scroll-on-keystroke"
#define KEY_SCROLL_ON_OUTPUT "scroll-on-output"
#define KEY_SILENT_BELL "silent-bell"
#define KEY_COPY_SELECTION "copy-selection"
#define KEY_TITLE_MODE "title-mode"
#define KEY_TITLE "title"
#define KEY_USE_CUSTOM_COMMAND "use-custom-command"
#define KEY_USE_CUSTOM_DEFAULT_SIZE "use-custom-default-size"
#define KEY_USE_SKEY "use-skey"
#define KEY_USE_URLS "use-urls"
#define KEY_USE_SYSTEM_FONT "use-system-font"
#define KEY_USE_THEME_COLORS "use-theme-colors"
#define KEY_VISIBLE_NAME "visible-name"
#define KEY_WORD_CHARS "word-chars"

/* Keep these in sync with the GSettings schema! */
#define DEFAULT_ALLOW_BOLD            (TRUE)
#define DEFAULT_BACKGROUND_COLOR      ("#FFFFDD")
#define DEFAULT_BOLD_COLOR_SAME_AS_FG (TRUE)
#define DEFAULT_BACKGROUND_DARKNESS   (0.5)
#define DEFAULT_BACKGROUND_IMAGE_FILE ("")
#define DEFAULT_BACKGROUND_IMAGE      (NULL)
#define DEFAULT_BACKGROUND_TYPE       (TERMINAL_BACKGROUND_SOLID)
#define DEFAULT_BACKSPACE_BINDING     (VTE_ERASE_ASCII_DELETE)
#define DEFAULT_CURSOR_BLINK_MODE     (VTE_CURSOR_BLINK_SYSTEM)
#define DEFAULT_CURSOR_SHAPE          (VTE_CURSOR_SHAPE_BLOCK)
#define DEFAULT_CUSTOM_COMMAND        ("")
#define DEFAULT_DEFAULT_SHOW_MENUBAR  (TRUE)
#define DEFAULT_DEFAULT_SIZE_COLUMNS  (80)
#define DEFAULT_DEFAULT_SIZE_ROWS     (24)
#define DEFAULT_DELETE_BINDING        (VTE_ERASE_DELETE_SEQUENCE)
#define DEFAULT_EXIT_ACTION           (TERMINAL_EXIT_CLOSE)
#define DEFAULT_FONT                  ("Monospace 12")
#define DEFAULT_FOREGROUND_COLOR      ("#000000")
#define DEFAULT_LOGIN_SHELL           (FALSE)
#define DEFAULT_NAME                  (NULL)
#define DEFAULT_PALETTE               (terminal_palettes[TERMINAL_PALETTE_TANGO])
#define DEFAULT_SCROLL_BACKGROUND     (TRUE)
#define DEFAULT_SCROLLBACK_LINES      (512)
#define DEFAULT_SCROLLBACK_UNLIMITED  (FALSE)
#define DEFAULT_SCROLLBAR_POSITION    (TERMINAL_SCROLLBAR_RIGHT)
#define DEFAULT_SCROLL_ON_KEYSTROKE   (TRUE)
#define DEFAULT_SCROLL_ON_OUTPUT      (FALSE)
#define DEFAULT_SILENT_BELL           (FALSE)
#define DEFAULT_COPY_SELECTION        (FALSE)
#define DEFAULT_TITLE_MODE            (TERMINAL_TITLE_REPLACE)
#define DEFAULT_TITLE                 (N_("Terminal"))
#define DEFAULT_USE_CUSTOM_COMMAND    (FALSE)
#define DEFAULT_USE_CUSTOM_DEFAULT_SIZE (FALSE)
#define DEFAULT_USE_SKEY              (TRUE)
#define DEFAULT_USE_URLS              (TRUE)
#define DEFAULT_USE_SYSTEM_FONT       (TRUE)
#define DEFAULT_USE_THEME_COLORS      (TRUE)
#define DEFAULT_VISIBLE_NAME          (N_("Unnamed"))
#define DEFAULT_WORD_CHARS            ("-A-Za-z0-9,./?%&#:_=+@~")

struct _TerminalProfilePrivate
{
	GValueArray *properties;
	gboolean *locked;

	GSettings *settings;
	char *profile_dir;

	GSList *dirty_pspecs;
	guint save_idle_id;

	GParamSpec *gsettings_notification_pspec;

	gboolean background_load_failed;

	guint forgotten : 1;
};

static const GdkRGBA terminal_palettes[TERMINAL_PALETTE_N_BUILTINS][TERMINAL_PALETTE_SIZE] =
{
	/* Tango palette */
	{
		{ 0,         0,        0,         1 },
		{ 0.8,       0,        0,         1 },
		{ 0.305882,  0.603922, 0.0235294, 1 },
		{ 0.768627,  0.627451, 0,         1 },
		{ 0.203922,  0.396078, 0.643137,  1 },
		{ 0.458824,  0.313725, 0.482353,  1 },
		{ 0.0235294, 0.596078, 0.603922,  1 },
		{ 0.827451,  0.843137, 0.811765,  1 },
		{ 0.333333,  0.341176, 0.32549,   1 },
		{ 0.937255,  0.160784, 0.160784,  1 },
		{ 0.541176,  0.886275, 0.203922,  1 },
		{ 0.988235,  0.913725, 0.309804,  1 },
		{ 0.447059,  0.623529, 0.811765,  1 },
		{ 0.678431,  0.498039, 0.658824,  1 },
		{ 0.203922,  0.886275, 0.886275,  1 },
		{ 0.933333,  0.933333, 0.92549,   1 },
	},

	/* Linux palette */
	{
		{ 0,        0,        0,        1 },
		{ 0.666667, 0,        0,        1 },
		{ 0,        0.666667, 0,        1 },
		{ 0.666667, 0.333333, 0,        1 },
		{ 0,        0,        0.666667, 1 },
		{ 0.666667, 0,        0.666667, 1 },
		{ 0,        0.666667, 0.666667, 1 },
		{ 0.666667, 0.666667, 0.666667, 1 },
		{ 0.333333, 0.333333, 0.333333, 1 },
		{ 1,        0.333333, 0.333333, 1 },
		{ 0.333333, 1,        0.333333, 1 },
		{ 1,        1,        0.333333, 1 },
		{ 0.333333, 0.333333, 1,        1 },
		{ 1,        0.333333, 1,        1 },
		{ 0.333333, 1,        1,        1 },
		{ 1,        1,        1,        1 },
	},

	/* XTerm palette */
	{
		{ 0,        0,        0,        1 },
		{ 0.803922, 0,        0,        1 },
		{ 0,        0.803922, 0,        1 },
		{ 0.803922, 0.803922, 0,        1 },
		{ 0.117647, 0.564706, 1,        1 },
		{ 0.803922, 0,        0.803922, 1 },
		{ 0,        0.803922, 0.803922, 1 },
		{ 0.898039, 0.898039, 0.898039, 1 },
		{ 0.298039, 0.298039, 0.298039, 1 },
		{ 1,        0,        0,        1 },
		{ 0,        1,        0,        1 },
		{ 1,        1,        0,        1 },
		{ 0.27451,  0.509804, 0.705882, 1 },
		{ 1,        0,        1,        1 },
		{ 0,        1,        1,        1 },
		{ 1,        1,        1,        1 },
	},

	/* RXVT palette */
	{
		{ 0,        0,        0,        1 },
		{ 0.803922, 0,        0,        1 },
		{ 0,        0.803922, 0,        1 },
		{ 0.803922, 0.803922, 0,        1 },
		{ 0,        0,        0.803922, 1 },
		{ 0.803922, 0,        0.803922, 1 },
		{ 0,        0.803922, 0.803922, 1 },
		{ 0.980392, 0.921569, 0.843137, 1 },
		{ 0.25098,  0.25098,  0.25098,  1 },
		{ 1, 0, 0, 1 },
		{ 0, 1, 0, 1 },
		{ 1, 1, 0, 1 },
		{ 0, 0, 1, 1 },
		{ 1, 0, 1, 1 },
		{ 0, 1, 1, 1 },
		{ 1, 1, 1, 1 },
	},

	/* Solarized palette (1.0.0beta2): http://ethanschoonover.com/solarized */
	{
		{ 0.02745,  0.211764, 0.258823, 1 },
		{ 0.862745, 0.196078, 0.184313, 1 },
		{ 0.521568, 0.6,      0,        1 },
		{ 0.709803, 0.537254, 0,        1 },
		{ 0.149019, 0.545098, 0.823529, 1 },
		{ 0.82745,  0.211764, 0.509803, 1 },
		{ 0.164705, 0.631372, 0.596078, 1 },
		{ 0.933333, 0.909803, 0.835294, 1 },
		{ 0,        0.168627, 0.211764, 1 },
		{ 0.796078, 0.294117, 0.086274, 1 },
		{ 0.345098, 0.431372, 0.458823, 1 },
		{ 0.396078, 0.482352, 0.513725, 1 },
		{ 0.513725, 0.580392, 0.588235, 1 },
		{ 0.423529, 0.443137, 0.768627, 1 },
		{ 0.57647,  0.631372, 0.631372, 1 },
		{ 0.992156, 0.964705, 0.890196, 1 },
	},
};

enum
{
    FORGOTTEN,
    LAST_SIGNAL
};

static void terminal_profile_finalize    (GObject              *object);
static void terminal_profile_set_property (GObject *object,
        guint prop_id,
        const GValue *value,
        GParamSpec *pspec);
static void ensure_pixbuf_property (TerminalProfile *profile,
                                    guint path_prop_id,
                                    guint pixbuf_prop_id,
                                    gboolean *load_failed);

static guint signals[LAST_SIGNAL] = { 0 };
static GQuark gsettings_key_quark;

G_DEFINE_TYPE_WITH_PRIVATE (TerminalProfile, terminal_profile, G_TYPE_OBJECT);

/* gdk_rgba_equal is too strict! */
static gboolean
rgba_equal (const GdkRGBA *a,
            const GdkRGBA *b)
{
    gdouble dr, dg, db, da;

    dr = a->red - b->red;
    dg = a->green - b->green;
    db = a->blue - b->blue;
    da = a->alpha - b->alpha;

    return (dr * dr + dg * dg + db * db + da * da) < 1e-4;
}

static gboolean
palette_cmp (const GdkRGBA *ca,
             const GdkRGBA *cb)
{
    guint i;

    for (i = 0; i < TERMINAL_PALETTE_SIZE; ++i)
        if (!rgba_equal (&ca[i], &cb[i]))
            return FALSE;

    return TRUE;
}

static GParamSpec *
get_pspec_from_name (TerminalProfile *profile,
                     const char *prop_name)
{
	TerminalProfileClass *klass = TERMINAL_PROFILE_GET_CLASS (profile);
	GParamSpec *pspec;

	pspec = g_object_class_find_property (G_OBJECT_CLASS (klass), prop_name);
	if (pspec &&
	        pspec->owner_type != TERMINAL_TYPE_PROFILE)
		pspec = NULL;

	return pspec;
}

static const GValue *
get_prop_value_from_prop_name (TerminalProfile *profile,
                               const char *prop_name)
{
	TerminalProfilePrivate *priv = profile->priv;
	GParamSpec *pspec;

	pspec = get_pspec_from_name (profile, prop_name);
	if (!pspec)
		return NULL;

	if (G_UNLIKELY (pspec->param_id == PROP_BACKGROUND_IMAGE))
		ensure_pixbuf_property (profile, PROP_BACKGROUND_IMAGE_FILE, PROP_BACKGROUND_IMAGE, &priv->background_load_failed);

	return g_value_array_get_nth (priv->properties, pspec->param_id);
}

static void
set_value_from_palette (GValue *ret_value,
                        const GdkRGBA *colors,
                        guint n_colors)
{
	GValueArray *array;
	guint i, max_n_colors;

	max_n_colors = MAX (n_colors, TERMINAL_PALETTE_SIZE);
	array = g_value_array_new (max_n_colors);
	for (i = 0; i < max_n_colors; ++i)
		g_value_array_append (array, NULL);

	for (i = 0; i < n_colors; ++i)
	{
		GValue *value = g_value_array_get_nth (array, i);

		g_value_init (value, GDK_TYPE_RGBA);
		g_value_set_boxed (value, &colors[i]);
	}

	/* If we haven't enough colours yet, fill up with the default palette */
	for (i = n_colors; i < TERMINAL_PALETTE_SIZE; ++i)
	{
		GValue *value = g_value_array_get_nth (array, i);

		g_value_init (value, GDK_TYPE_RGBA);
		g_value_set_boxed (value, &DEFAULT_PALETTE[i]);
	}

	g_value_take_boxed (ret_value, array);
}

static int
values_equal (GParamSpec *pspec,
              const GValue *va,
              const GValue *vb)
{
	/* g_param_values_cmp isn't good enough for some types, since e.g.
	 * it compares colours and font descriptions by pointer value, not
	 * with the correct compare functions. Providing extra
	 * PangoParamSpecFontDescription and GdkParamSpecColor wouldn't
	 * have fixed this either, since it's unclear how to _order_ them.
	 * Luckily we only need to check them for equality here.
	 */

	if (g_param_values_cmp (pspec, va, vb) == 0)
		return TRUE;

	if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
		return rgba_equal (g_value_get_boxed (va), g_value_get_boxed (vb));

	if (G_PARAM_SPEC_VALUE_TYPE (pspec) == PANGO_TYPE_FONT_DESCRIPTION)
		return pango_font_description_equal (g_value_get_boxed (va), g_value_get_boxed (vb));

	if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec) &&
	        G_PARAM_SPEC_VALUE_TYPE (G_PARAM_SPEC_VALUE_ARRAY (pspec)->element_spec) == GDK_TYPE_RGBA)
	{
		GValueArray *ara, *arb;
		guint i;

		ara = g_value_get_boxed (va);
		arb = g_value_get_boxed (vb);

		if (!ara || !arb || ara->n_values != arb->n_values)
			return FALSE;

		for (i = 0; i < ara->n_values; ++i)
			if (!rgba_equal (g_value_get_boxed (g_value_array_get_nth (ara, i)),
			                      g_value_get_boxed (g_value_array_get_nth (arb, i))))
				return FALSE;

		return TRUE;
	}

	return FALSE;
}

static void
ensure_pixbuf_property (TerminalProfile *profile,
                        guint path_prop_id,
                        guint pixbuf_prop_id,
                        gboolean *load_failed)
{
	TerminalProfilePrivate *priv = profile->priv;
	GValue *path_value, *pixbuf_value;
	GdkPixbuf *pixbuf;
	const char *path_utf8;
	char *path;
	GError *error = NULL;

	pixbuf_value = g_value_array_get_nth (priv->properties, pixbuf_prop_id);

	pixbuf = g_value_get_object (pixbuf_value);
	if (pixbuf)
		return;

	if (*load_failed)
		return;

	path_value = g_value_array_get_nth (priv->properties, path_prop_id);
	path_utf8 = g_value_get_string (path_value);
	if (!path_utf8 || !path_utf8[0])
		goto failed;

	path = g_filename_from_utf8 (path_utf8, -1, NULL, NULL, NULL);
	if (!path)
		goto failed;

	pixbuf = gdk_pixbuf_new_from_file (path, &error);
	if (!pixbuf)
	{
		_terminal_debug_print (TERMINAL_DEBUG_PROFILE,
		                       "Failed to load image \"%s\": %s\n",
		                       path, error->message);

		g_error_free (error);
		g_free (path);
		goto failed;
	}

	g_value_take_object (pixbuf_value, pixbuf);
	g_free (path);
	return;

failed:
	*load_failed = TRUE;
}

static void
terminal_profile_reset_property_internal (TerminalProfile *profile,
        GParamSpec *pspec,
        gboolean notify)
{
	TerminalProfilePrivate *priv = profile->priv;
	GValue value_ = { 0, };
	GValue *value;

	if (notify)
	{
		value = &value_;
		g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));
	}
	else
		value = g_value_array_get_nth (priv->properties, pspec->param_id);
	g_assert (value != NULL);

	/* A few properties don't have defaults via the param spec; set them explicitly */
	switch (pspec->param_id)
	{
	case PROP_FOREGROUND_COLOR:
	case PROP_BOLD_COLOR:
		g_value_set_boxed (value, &DEFAULT_FOREGROUND_COLOR);
		break;

	case PROP_BACKGROUND_COLOR:
		g_value_set_boxed (value, &DEFAULT_BACKGROUND_COLOR);
		break;

	case PROP_FONT:
		g_value_take_boxed (value, pango_font_description_from_string (DEFAULT_FONT));
		break;

	case PROP_PALETTE:
		set_value_from_palette (value, DEFAULT_PALETTE, TERMINAL_PALETTE_SIZE);
		break;

	default:
		g_param_value_set_default (pspec, value);
		break;
	}

	if (notify)
	{
		g_object_set_property (G_OBJECT (profile), pspec->name, value);
		g_value_unset (value);
	}
}

static void
terminal_profile_gsettings_notify_cb (GSettings *settings,
                                      gchar *key,
                                      gpointer     user_data)
{
	TerminalProfile *profile = TERMINAL_PROFILE (user_data);
	TerminalProfilePrivate *priv = profile->priv;
	TerminalProfileClass *klass;
	GVariant *settings_value;
	GParamSpec *pspec;
	GValue value = { 0, };
	gboolean equal;
	gboolean force_set = FALSE;

	if (!key) return;

	_terminal_debug_print (TERMINAL_DEBUG_PROFILE,
	                       "GSettings notification for key %s [%s]\n",
	                       key,
	                       g_settings_is_writable (settings, key) ? "writable" : "LOCKED");

	klass = TERMINAL_PROFILE_GET_CLASS (profile);
	pspec = g_hash_table_lookup (klass->gsettings_keys, key);
	if (!pspec)
		return; /* ignore unknown keys, for future extensibility */

	priv->locked[pspec->param_id] = !g_settings_is_writable (settings, key);

	settings_value = g_settings_get_value (settings, key);
	if (!settings_value)
		return;

	g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));

	if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
	{
		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_BOOLEAN))
			goto out;

		g_value_set_boolean (&value, g_variant_get_boolean (settings_value));
	}
	else if (G_IS_PARAM_SPEC_STRING (pspec))
	{
		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_STRING))
			goto out;

		g_value_set_string (&value, g_variant_get_string (settings_value, NULL));
	}
	else if (G_IS_PARAM_SPEC_ENUM (pspec))
	{

		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_STRING))
			goto out;

		g_value_set_enum (&value, g_settings_get_enum (settings, key));
	}
	else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
	{
		GdkRGBA color;

		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_STRING))
			goto out;

		if (!gdk_rgba_parse (&color, g_variant_get_string (settings_value, NULL)))
			goto out;

		g_value_set_boxed (&value, &color);
	}
	else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == PANGO_TYPE_FONT_DESCRIPTION)
	{
		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_STRING))
			goto out;

		g_value_take_boxed (&value, pango_font_description_from_string (g_variant_get_string (settings_value, NULL)));
	}
	else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
	{
		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_DOUBLE))
			goto out;

		g_value_set_double (&value, g_variant_get_double (settings_value));
	}
	else if (G_IS_PARAM_SPEC_INT (pspec))
	{
		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_INT16) &&
		    !g_variant_is_of_type (settings_value, G_VARIANT_TYPE_INT32) &&
		    !g_variant_is_of_type (settings_value, G_VARIANT_TYPE_INT64))
			goto out;

		g_value_set_int (&value, g_settings_get_int(settings, key));
	}
	else if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec) &&
	         G_PARAM_SPEC_VALUE_TYPE (G_PARAM_SPEC_VALUE_ARRAY (pspec)->element_spec) == GDK_TYPE_RGBA)
	{
		char **color_strings;
		GdkRGBA *colors;
		int n_colors, i;

		if (!g_variant_is_of_type (settings_value, G_VARIANT_TYPE_STRING))
			goto out;

		color_strings = g_strsplit (g_variant_get_string (settings_value, NULL), ":", -1);
		if (!color_strings)
			goto out;

		n_colors = g_strv_length (color_strings);
		colors = g_new0 (GdkRGBA, n_colors);
		for (i = 0; i < n_colors; ++i)
		{
			if (!gdk_rgba_parse (&colors[i], color_strings[i]))
				continue; /* ignore errors */
		}
		g_strfreev (color_strings);

		/* We continue even with a palette size != TERMINAL_PALETTE_SIZE,
		 * so we can change the palette size in future versions without
		 * causing too many issues.
		 */
		set_value_from_palette (&value, colors, n_colors);
		g_free (colors);
	}
	else
	{
		g_printerr ("Unhandled value type %s of pspec %s\n", g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)), pspec->name);
		goto out;
	}

	if (g_param_value_validate (pspec, &value))
	{
		_terminal_debug_print (TERMINAL_DEBUG_PROFILE,
		                       "Invalid value in GSettings for key %s was changed to comply with pspec %s\n",
		                       key, pspec->name);

		force_set = TRUE;
	}

	/* Only set the property if the value is different than our current value,
	 * so we don't go into an infinite loop.
	 */
	equal = values_equal (pspec, &value, g_value_array_get_nth (priv->properties, pspec->param_id));
#ifdef MATE_ENABLE_DEBUG
	_TERMINAL_DEBUG_IF (TERMINAL_DEBUG_PROFILE)
	{
		if (!equal)
			_terminal_debug_print (TERMINAL_DEBUG_PROFILE,
			                       "Setting property %s to a different value\n"
			                       "  now: %s\n"
			                       "  new: %s\n",
			                       pspec->name,
			                       g_strdup_value_contents (g_value_array_get_nth (priv->properties, pspec->param_id)),
			                       g_strdup_value_contents (&value));
	}
#endif

	if (!equal || force_set)
	{
		priv->gsettings_notification_pspec = pspec;
		g_object_set_property (G_OBJECT (profile), pspec->name, &value);
		priv->gsettings_notification_pspec = NULL;
	}

out:
	/* FIXME: if we arrive here through goto in the error cases,
	 * should we maybe reset the property to its default value?
	 */

	g_value_unset (&value);
	g_variant_unref (settings_value);
}

static void
terminal_profile_gsettings_changeset_add (TerminalProfile *profile,
        GSettings *changeset,
        GParamSpec *pspec)
{
	TerminalProfilePrivate *priv = profile->priv;
	char *key;
	const GValue *value;

	/* FIXME: do this? */
#if 0
	if (priv->locked[pspec->param_id])
		return;

	if (!g_settings_is_writable (priv->settings, gsettings_key, NULL))
		return;
#endif

	key = g_param_spec_get_qdata (pspec, gsettings_key_quark);
	if (!key)
		return;

	value = g_value_array_get_nth (priv->properties, pspec->param_id);

	_terminal_debug_print (TERMINAL_DEBUG_PROFILE,
	                       "Adding pspec %s with value %s to the GSettings changeset\n",
	                       pspec->name, g_strdup_value_contents (value));

	if (G_IS_PARAM_SPEC_BOOLEAN (pspec))
		g_settings_set_boolean (changeset, key, g_value_get_boolean (value));
	else if (G_IS_PARAM_SPEC_STRING (pspec))
	{
		const char *str;

		str = g_value_get_string (value);
		g_settings_set_string (changeset, key, str ? str : "");
	}
	else if (G_IS_PARAM_SPEC_ENUM (pspec))
	{
		const GEnumValue *eval;

		eval = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, g_value_get_enum (value));

		g_settings_set_enum (changeset, key, eval->value);
	}
	else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == GDK_TYPE_RGBA)
	{
		GdkRGBA *color;
		char str[16];

		color = g_value_get_boxed (value);
		if (!color)
			goto cleanup;

		g_snprintf (str, sizeof (str),
		            "#%04X%04X%04X",
		            (guint) (color->red * 65535),
		            (guint) (color->green * 65535),
		            (guint) (color->blue * 65535));

		g_settings_set_string (changeset, key, str);
	}
	else if (G_PARAM_SPEC_VALUE_TYPE (pspec) == PANGO_TYPE_FONT_DESCRIPTION)
	{
		PangoFontDescription *font_desc;
		char *font;

		font_desc = g_value_get_boxed (value);
		if (!font_desc)
			goto cleanup;

		font = pango_font_description_to_string (font_desc);
		g_settings_set_string (changeset, key, font);
		g_free (font);
	}
	else if (G_IS_PARAM_SPEC_DOUBLE (pspec))
		g_settings_set_double (changeset, key, g_value_get_double (value));
	else if (G_IS_PARAM_SPEC_INT (pspec))
		g_settings_set_int (changeset, key, g_value_get_int (value));
	else if (G_IS_PARAM_SPEC_VALUE_ARRAY (pspec) &&
	         G_PARAM_SPEC_VALUE_TYPE (G_PARAM_SPEC_VALUE_ARRAY (pspec)->element_spec) == GDK_TYPE_RGBA)
	{
		GValueArray *array;
		GString *string;
		guint n_colors, i;

		/* We need to do this ourselves, because the gtk_color_selection_palette_to_string
		 * does not carry all the bytes, and xterm's palette is messed up...
		 */

		array = g_value_get_boxed (value);
		if (!array)
			goto cleanup;

		n_colors = array->n_values;
		string = g_string_sized_new (n_colors * (1 /* # */ + 3 * 4) + n_colors /* : separators and terminating \0 */);
		for (i = 0; i < n_colors; ++i)
		{
			GdkRGBA *color;

			if (i > 0)
				g_string_append_c (string, ':');

			color = g_value_get_boxed (g_value_array_get_nth (array, i));
			if (!color)
				continue;

			g_string_append_printf (string,
			                        "#%04X%04X%04X",
			                        (guint) (color->red * 65535),
			                        (guint) (color->green * 65535),
			                        (guint) (color->blue * 65535));
		}

		g_settings_set_string (changeset, key, string->str);
		g_string_free (string, TRUE);
	}
	else
		g_printerr ("Unhandled value type %s of pspec %s\n", g_type_name (G_PARAM_SPEC_VALUE_TYPE (pspec)), pspec->name);

cleanup:
	return;
}

static void
terminal_profile_save (TerminalProfile *profile)
{
	TerminalProfilePrivate *priv = profile->priv;
	GSettings *changeset;
	GSList *l;
	gchar *concat;

	priv->save_idle_id = 0;
	concat = g_strconcat (CONF_PROFILE_PREFIX, priv->profile_dir,"/", NULL);
	changeset = g_settings_new_with_path (CONF_PROFILE_SCHEMA, concat);
	g_free (concat);
	g_settings_delay (changeset);

	for (l = priv->dirty_pspecs; l != NULL; l = l->next)
	{
		GParamSpec *pspec = (GParamSpec *) l->data;

		if (pspec->owner_type != TERMINAL_TYPE_PROFILE)
			continue;

		if ((pspec->flags & G_PARAM_WRITABLE) == 0)
			continue;

		terminal_profile_gsettings_changeset_add (profile, changeset, pspec);
	}

	g_slist_free (priv->dirty_pspecs);
	priv->dirty_pspecs = NULL;

	g_settings_apply (changeset);
	g_object_unref (changeset);
}

static gboolean
terminal_profile_save_idle_cb (TerminalProfile *profile)
{
	terminal_profile_save (profile);

	/* don't run again */
	return FALSE;
}

static void
terminal_profile_schedule_save (TerminalProfile *profile,
                                GParamSpec *pspec)
{
	TerminalProfilePrivate *priv = profile->priv;

	g_assert (pspec != NULL);

	if (!g_slist_find (priv->dirty_pspecs, pspec))
		priv->dirty_pspecs = g_slist_prepend (priv->dirty_pspecs, pspec);

	if (priv->save_idle_id != 0)
		return;

	priv->save_idle_id = g_idle_add ((GSourceFunc) terminal_profile_save_idle_cb, profile);
}

static void
terminal_profile_init (TerminalProfile *profile)
{
	TerminalProfilePrivate *priv;
	GObjectClass *object_class;
	GParamSpec **pspecs;
	guint n_pspecs, i;

	priv = profile->priv = terminal_profile_get_instance_private (profile);

	priv->gsettings_notification_pspec = NULL;
	priv->locked = g_new0 (gboolean, LAST_PROP);

	priv->properties = g_value_array_new (LAST_PROP);
	for (i = 0; i < LAST_PROP; ++i)
		g_value_array_append (priv->properties, NULL);

	pspecs = g_object_class_list_properties (G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (profile)), &n_pspecs);
	for (i = 0; i < n_pspecs; ++i)
	{
		GParamSpec *pspec = pspecs[i];
		GValue *value;

		if (pspec->owner_type != TERMINAL_TYPE_PROFILE)
			continue;

		g_assert (pspec->param_id < LAST_PROP);
		value = g_value_array_get_nth (priv->properties, pspec->param_id);
		g_value_init (value, pspec->value_type);
		g_param_value_set_default (pspec, value);
	}

	g_free (pspecs);

	/* A few properties don't have defaults via the param spec; set them explicitly */
	object_class = G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (profile));
	terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_FOREGROUND_COLOR), FALSE);
	terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_BOLD_COLOR), FALSE);
	terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_BACKGROUND_COLOR), FALSE);
	terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_FONT), FALSE);
	terminal_profile_reset_property_internal (profile, g_object_class_find_property (object_class, TERMINAL_PROFILE_PALETTE), FALSE);
}

static GObject *
terminal_profile_constructor (GType type,
                              guint n_construct_properties,
                              GObjectConstructParam *construct_params)
{
	GObject *object;
	TerminalProfile *profile;
	TerminalProfilePrivate *priv;
	const char *name;
	GParamSpec **pspecs;
	guint n_pspecs, i;
	gchar *concat;

	object = G_OBJECT_CLASS (terminal_profile_parent_class)->constructor
	         (type, n_construct_properties, construct_params);

	profile = TERMINAL_PROFILE (object);
	priv = profile->priv;

	name = g_value_get_string (g_value_array_get_nth (priv->properties, PROP_NAME));
	g_assert (name != NULL);

	concat = g_strconcat (CONF_PROFILE_PREFIX, name, "/", NULL);
	priv->settings = g_settings_new_with_path (CONF_PROFILE_SCHEMA, concat);
	g_assert (priv->settings != NULL);
	g_free (concat);
	concat = g_strconcat("changed::", priv->profile_dir, "/", NULL);
	g_signal_connect (priv->settings,
			  concat,
			  G_CALLBACK(terminal_profile_gsettings_notify_cb),
			  profile);

	g_free (concat);

	/* Now load those properties from GSettings that were not set as construction params */
	pspecs = g_object_class_list_properties (G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (profile)), &n_pspecs);
	for (i = 0; i < n_pspecs; ++i)
	{
		GParamSpec *pspec = pspecs[i];
		guint j;
		gboolean is_construct = FALSE;
		char *key;

		if (pspec->owner_type != TERMINAL_TYPE_PROFILE)
			continue;

		if ((pspec->flags & G_PARAM_WRITABLE) == 0 ||
		        (pspec->flags & G_PARAM_CONSTRUCT_ONLY) != 0)
			continue;

		for (j = 0; j < n_construct_properties; ++j)
			if (pspec == construct_params[j].pspec)
			{
				is_construct = TRUE;
				break;
			}

		if (is_construct)
			continue;

		key = g_param_spec_get_qdata (pspec, gsettings_key_quark);
		if (!key)
			continue;

		terminal_profile_gsettings_notify_cb (priv->settings, key,  profile);
	}

	g_free (pspecs);

	return object;
}

static void
terminal_profile_finalize (GObject *object)
{
	TerminalProfile *profile = TERMINAL_PROFILE (object);
	TerminalProfilePrivate *priv = profile->priv;

	g_signal_handlers_disconnect_by_func (priv->settings,
		G_CALLBACK(terminal_profile_gsettings_notify_cb),
		profile);

	if (priv->save_idle_id)
	{
		g_source_remove (priv->save_idle_id);

		/* Save now */
		terminal_profile_save (profile);
	}

	_terminal_profile_forget (profile);

	g_object_unref (priv->settings);

	g_free (priv->profile_dir);
	g_free (priv->locked);
	g_value_array_free (priv->properties);

	G_OBJECT_CLASS (terminal_profile_parent_class)->finalize (object);
}

static void
terminal_profile_get_property (GObject *object,
                               guint prop_id,
                               GValue *value,
                               GParamSpec *pspec)
{
	TerminalProfile *profile = TERMINAL_PROFILE (object);
	TerminalProfilePrivate *priv = profile->priv;

	if (prop_id == 0 || prop_id >= LAST_PROP)
	{
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		return;
	}

	/* Note: When adding things here, do the same in get_prop_value_from_prop_name! */
	switch (prop_id)
	{
	case PROP_BACKGROUND_IMAGE:
		ensure_pixbuf_property (profile, PROP_BACKGROUND_IMAGE_FILE, PROP_BACKGROUND_IMAGE, &priv->background_load_failed);
		break;
	default:
		break;
	}

	g_value_copy (g_value_array_get_nth (priv->properties, prop_id), value);
}

static void
terminal_profile_set_property (GObject *object,
                               guint prop_id,
                               const GValue *value,
                               GParamSpec *pspec)
{
	TerminalProfile *profile = TERMINAL_PROFILE (object);
	TerminalProfilePrivate *priv = profile->priv;
	GValue *prop_value;

	if (prop_id == 0 || prop_id >= LAST_PROP)
	{
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		return;
	}

	prop_value = g_value_array_get_nth (priv->properties, prop_id);

	/* Preprocessing */
	switch (prop_id)
	{
#if 0
	case PROP_FONT:
	{
		PangoFontDescription *font_desc, *new_font_desc;

		font_desc = g_value_get_boxed (prop_value);
		new_font_desc = g_value_get_boxed (value);

		if (font_desc && new_font_desc)
		{
			/* Merge in case the new string isn't complete enough to load a font */
			pango_font_description_merge (font_desc, new_font_desc, TRUE);
			pango_font_description_free (new_font_desc);
			break;
		}

		/* fall-through */
	}
#endif
	default:
		g_value_copy (value, prop_value);
		break;
	}

	/* Postprocessing */
	switch (prop_id)
	{
	case PROP_NAME:
	{
		const char *name = g_value_get_string (value);

		g_assert (name != NULL);
		priv->profile_dir = g_strdup (name);
		if (priv->settings != NULL) {
			gchar *concat;
			g_signal_handlers_disconnect_by_func (priv->settings,
							      G_CALLBACK(terminal_profile_gsettings_notify_cb),
							      profile);
			g_object_unref (priv->settings);
			concat=  g_strconcat (CONF_PROFILE_PREFIX, priv->profile_dir, "/", NULL);
			priv->settings = g_settings_new_with_path (CONF_PROFILE_SCHEMA, concat);
			g_free (concat);
			concat = g_strconcat("changed::", priv->profile_dir, "/", NULL);
			g_signal_connect (priv->settings,
					  concat,
					  G_CALLBACK(terminal_profile_gsettings_notify_cb),
					  profile);
			g_free (concat);
		}
		break;
	}

	case PROP_BACKGROUND_IMAGE_FILE:
		/* Clear the cached image */
		g_value_set_object (g_value_array_get_nth (priv->properties, PROP_BACKGROUND_IMAGE), NULL);
		priv->background_load_failed = FALSE;
		g_object_notify (object, TERMINAL_PROFILE_BACKGROUND_IMAGE);
		break;

	default:
		break;
	}
}

static void
terminal_profile_notify (GObject *object,
                         GParamSpec *pspec)
{
	TerminalProfilePrivate *priv = TERMINAL_PROFILE (object)->priv;
	void (* notify) (GObject *, GParamSpec *) = G_OBJECT_CLASS (terminal_profile_parent_class)->notify;

	_terminal_debug_print (TERMINAL_DEBUG_PROFILE,
	                       "Property notification for prop %s\n",
	                       pspec->name);

	if (notify)
		notify (object, pspec);

	if (pspec->owner_type == TERMINAL_TYPE_PROFILE &&
	        (pspec->flags & G_PARAM_WRITABLE) &&
	        g_param_spec_get_qdata (pspec, gsettings_key_quark) != NULL &&
	        pspec != priv->gsettings_notification_pspec)
		terminal_profile_schedule_save (TERMINAL_PROFILE (object), pspec);
}

static void
terminal_profile_class_init (TerminalProfileClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	gsettings_key_quark = g_quark_from_static_string ("GT::GSettingsKey");

	object_class->constructor = terminal_profile_constructor;
	object_class->finalize = terminal_profile_finalize;
	object_class->get_property = terminal_profile_get_property;
	object_class->set_property = terminal_profile_set_property;
	object_class->notify = terminal_profile_notify;

	signals[FORGOTTEN] =
	    g_signal_new ("forgotten",
	                  G_OBJECT_CLASS_TYPE (object_class),
	                  G_SIGNAL_RUN_LAST,
	                  G_STRUCT_OFFSET (TerminalProfileClass, forgotten),
	                  NULL, NULL,
	                  g_cclosure_marshal_VOID__VOID,
	                  G_TYPE_NONE, 0);

	/* gsettings_key -> pspec hash */
	klass->gsettings_keys = g_hash_table_new (g_str_hash, g_str_equal);

#define TERMINAL_PROFILE_PSPEC_STATIC (G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB)

#define TERMINAL_PROFILE_PROPERTY(propId, propSpec, propGSettings) \
{\
  GParamSpec *pspec = propSpec;\
  g_object_class_install_property (object_class, propId, pspec);\
\
  if (propGSettings)\
    {\
      g_param_spec_set_qdata (pspec, gsettings_key_quark, (gpointer) propGSettings);\
      g_hash_table_insert (klass->gsettings_keys, (gpointer) propGSettings, pspec);\
    }\
}

#define TERMINAL_PROFILE_PROPERTY_BOOLEAN(prop, propDefault, propGSettings) \
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_boolean (TERMINAL_PROFILE_##prop, NULL, NULL,\
                          propDefault,\
                          G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_BOXED(prop, propType, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_boxed (TERMINAL_PROFILE_##prop, NULL, NULL,\
                        propType,\
                        G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_DOUBLE(prop, propMin, propMax, propDefault, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_double (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propMin, propMax, propDefault,\
                         G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_ENUM(prop, propType, propDefault, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_enum (TERMINAL_PROFILE_##prop, NULL, NULL,\
                       propType, propDefault,\
                       G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_INT(prop, propMin, propMax, propDefault, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_int (TERMINAL_PROFILE_##prop, NULL, NULL,\
                      propMin, propMax, propDefault,\
                      G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

	/* these are all read-only */
#define TERMINAL_PROFILE_PROPERTY_OBJECT(prop, propType, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_object (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propType,\
                         G_PARAM_READABLE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_STRING(prop, propDefault, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_string (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propDefault,\
                         G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_STRING_CO(prop, propDefault, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_string (TERMINAL_PROFILE_##prop, NULL, NULL,\
                         propDefault,\
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

#define TERMINAL_PROFILE_PROPERTY_VALUE_ARRAY_BOXED(prop, propElementName, propElementType, propGSettings)\
  TERMINAL_PROFILE_PROPERTY (PROP_##prop,\
    g_param_spec_value_array (TERMINAL_PROFILE_##prop, NULL, NULL,\
                              g_param_spec_boxed (propElementName, NULL, NULL,\
                                                  propElementType, \
                                                  G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
                              G_PARAM_READWRITE | TERMINAL_PROFILE_PSPEC_STATIC),\
    propGSettings)

	TERMINAL_PROFILE_PROPERTY_BOOLEAN (ALLOW_BOLD, DEFAULT_ALLOW_BOLD, KEY_ALLOW_BOLD);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (BOLD_COLOR_SAME_AS_FG, DEFAULT_BOLD_COLOR_SAME_AS_FG, KEY_BOLD_COLOR_SAME_AS_FG);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (DEFAULT_SHOW_MENUBAR, DEFAULT_DEFAULT_SHOW_MENUBAR, KEY_DEFAULT_SHOW_MENUBAR);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (LOGIN_SHELL, DEFAULT_LOGIN_SHELL, KEY_LOGIN_SHELL);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLL_BACKGROUND, DEFAULT_SCROLL_BACKGROUND, KEY_SCROLL_BACKGROUND);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLLBACK_UNLIMITED, DEFAULT_SCROLLBACK_UNLIMITED, KEY_SCROLLBACK_UNLIMITED);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLL_ON_KEYSTROKE, DEFAULT_SCROLL_ON_KEYSTROKE, KEY_SCROLL_ON_KEYSTROKE);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (SCROLL_ON_OUTPUT, DEFAULT_SCROLL_ON_OUTPUT, KEY_SCROLL_ON_OUTPUT);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (SILENT_BELL, DEFAULT_SILENT_BELL, KEY_SILENT_BELL);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (COPY_SELECTION, DEFAULT_COPY_SELECTION, KEY_COPY_SELECTION);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_CUSTOM_COMMAND, DEFAULT_USE_CUSTOM_COMMAND, KEY_USE_CUSTOM_COMMAND);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_CUSTOM_DEFAULT_SIZE, DEFAULT_USE_CUSTOM_DEFAULT_SIZE, KEY_USE_CUSTOM_DEFAULT_SIZE);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_SKEY, DEFAULT_USE_SKEY, KEY_USE_SKEY);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_URLS, DEFAULT_USE_URLS, KEY_USE_URLS);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_SYSTEM_FONT, DEFAULT_USE_SYSTEM_FONT, KEY_USE_SYSTEM_FONT);
	TERMINAL_PROFILE_PROPERTY_BOOLEAN (USE_THEME_COLORS, DEFAULT_USE_THEME_COLORS, KEY_USE_THEME_COLORS);

	TERMINAL_PROFILE_PROPERTY_BOXED (BACKGROUND_COLOR, GDK_TYPE_RGBA, KEY_BACKGROUND_COLOR);
	TERMINAL_PROFILE_PROPERTY_BOXED (BOLD_COLOR, GDK_TYPE_RGBA, KEY_BOLD_COLOR);
	TERMINAL_PROFILE_PROPERTY_BOXED (FONT, PANGO_TYPE_FONT_DESCRIPTION, KEY_FONT);
	TERMINAL_PROFILE_PROPERTY_BOXED (FOREGROUND_COLOR, GDK_TYPE_RGBA, KEY_FOREGROUND_COLOR);

	/* 0.0 = normal bg, 1.0 = all black bg, 0.5 = half darkened */
	TERMINAL_PROFILE_PROPERTY_DOUBLE (BACKGROUND_DARKNESS, 0.0, 1.0, DEFAULT_BACKGROUND_DARKNESS, KEY_BACKGROUND_DARKNESS);

	TERMINAL_PROFILE_PROPERTY_ENUM (BACKGROUND_TYPE, TERMINAL_TYPE_BACKGROUND_TYPE, DEFAULT_BACKGROUND_TYPE, KEY_BACKGROUND_TYPE);
	TERMINAL_PROFILE_PROPERTY_ENUM (BACKSPACE_BINDING,  VTE_TYPE_ERASE_BINDING, DEFAULT_BACKSPACE_BINDING, KEY_BACKSPACE_BINDING);
	TERMINAL_PROFILE_PROPERTY_ENUM (CURSOR_BLINK_MODE, VTE_TYPE_CURSOR_BLINK_MODE, DEFAULT_CURSOR_BLINK_MODE, KEY_CURSOR_BLINK_MODE);
	TERMINAL_PROFILE_PROPERTY_ENUM (CURSOR_SHAPE, VTE_TYPE_CURSOR_SHAPE, DEFAULT_CURSOR_SHAPE, KEY_CURSOR_SHAPE);
	TERMINAL_PROFILE_PROPERTY_ENUM (DELETE_BINDING, VTE_TYPE_ERASE_BINDING, DEFAULT_DELETE_BINDING, KEY_DELETE_BINDING);
	TERMINAL_PROFILE_PROPERTY_ENUM (EXIT_ACTION, TERMINAL_TYPE_EXIT_ACTION, DEFAULT_EXIT_ACTION, KEY_EXIT_ACTION);
	TERMINAL_PROFILE_PROPERTY_ENUM (SCROLLBAR_POSITION, TERMINAL_TYPE_SCROLLBAR_POSITION, DEFAULT_SCROLLBAR_POSITION, KEY_SCROLLBAR_POSITION);
	TERMINAL_PROFILE_PROPERTY_ENUM (TITLE_MODE, TERMINAL_TYPE_TITLE_MODE, DEFAULT_TITLE_MODE, KEY_TITLE_MODE);

	TERMINAL_PROFILE_PROPERTY_INT (DEFAULT_SIZE_COLUMNS, 1, 1024, DEFAULT_DEFAULT_SIZE_COLUMNS, KEY_DEFAULT_SIZE_COLUMNS);
	TERMINAL_PROFILE_PROPERTY_INT (DEFAULT_SIZE_ROWS, 1, 1024, DEFAULT_DEFAULT_SIZE_ROWS, KEY_DEFAULT_SIZE_ROWS);
	TERMINAL_PROFILE_PROPERTY_INT (SCROLLBACK_LINES, 1, G_MAXINT, DEFAULT_SCROLLBACK_LINES, KEY_SCROLLBACK_LINES);

	TERMINAL_PROFILE_PROPERTY_OBJECT (BACKGROUND_IMAGE, GDK_TYPE_PIXBUF, NULL);

	TERMINAL_PROFILE_PROPERTY_STRING_CO (NAME, DEFAULT_NAME, NULL);
	TERMINAL_PROFILE_PROPERTY_STRING (BACKGROUND_IMAGE_FILE, DEFAULT_BACKGROUND_IMAGE_FILE, KEY_BACKGROUND_IMAGE_FILE);
	TERMINAL_PROFILE_PROPERTY_STRING (CUSTOM_COMMAND, DEFAULT_CUSTOM_COMMAND, KEY_CUSTOM_COMMAND);
	TERMINAL_PROFILE_PROPERTY_STRING (TITLE, _(DEFAULT_TITLE), KEY_TITLE);
	TERMINAL_PROFILE_PROPERTY_STRING (VISIBLE_NAME, _(DEFAULT_VISIBLE_NAME), KEY_VISIBLE_NAME);
	TERMINAL_PROFILE_PROPERTY_STRING (WORD_CHARS, DEFAULT_WORD_CHARS, KEY_WORD_CHARS);

	TERMINAL_PROFILE_PROPERTY_VALUE_ARRAY_BOXED (PALETTE, "palette-color", GDK_TYPE_RGBA, KEY_PALETTE);
}

/* Semi-Public API */

TerminalProfile*
_terminal_profile_new (const char *name)
{
	return g_object_new (TERMINAL_TYPE_PROFILE,
	                     "name", name,
	                     NULL);
}

void
_terminal_profile_forget (TerminalProfile *profile)
{
	TerminalProfilePrivate *priv = profile->priv;

	if (!priv->forgotten)
	{
		priv->forgotten = TRUE;

		g_signal_emit (G_OBJECT (profile), signals[FORGOTTEN], 0);
	}
}

gboolean
_terminal_profile_get_forgotten (TerminalProfile *profile)
{
	return profile->priv->forgotten;
}

TerminalProfile *
_terminal_profile_clone (TerminalProfile *base_profile,
                         const char      *visible_name)
{
	TerminalApp *app = terminal_app_get ();
	GObject *base_object = G_OBJECT (base_profile);
	TerminalProfilePrivate *new_priv;
	char profile_name[32];
	GParameter *params;
	GParamSpec **pspecs;
	guint n_pspecs, i, n_params, profile_num;
	TerminalProfile *new_profile;

	g_object_ref (base_profile);

	profile_num = 0;
	do
	{
		g_snprintf (profile_name, sizeof (profile_name), "profile%u", profile_num++);
	}
	while (terminal_app_get_profile_by_name (app, profile_name) != NULL);

	/* Now we have an unused profile name */
	pspecs = g_object_class_list_properties (G_OBJECT_CLASS (TERMINAL_PROFILE_GET_CLASS (base_profile)), &n_pspecs);

	params = g_newa (GParameter, n_pspecs);
	n_params = 0;

	for (i = 0; i < n_pspecs; ++i)
	{
		GParamSpec *pspec = pspecs[i];
		GValue *value;

		if (pspec->owner_type != TERMINAL_TYPE_PROFILE ||
		        (pspec->flags & G_PARAM_WRITABLE) == 0)
			continue;

		params[n_params].name = pspec->name;

		value = &params[n_params].value;
		G_VALUE_TYPE (value) = 0;
		g_value_init (value, G_PARAM_SPEC_VALUE_TYPE (pspec));

		if (pspec->name == I_(TERMINAL_PROFILE_NAME))
			g_value_set_static_string (value, profile_name);
		else if (pspec->name == I_(TERMINAL_PROFILE_VISIBLE_NAME))
			g_value_set_static_string (value, visible_name);
		else
			g_object_get_property (base_object, pspec->name, value);

		++n_params;
	}

	new_profile = g_object_newv (TERMINAL_TYPE_PROFILE, n_params, params);

	g_object_unref (base_profile);

	for (i = 0; i < n_params; ++i)
		g_value_unset (&params[i].value);

	/* Flush the new profile to GSettings */
	new_priv = new_profile->priv;

	g_slist_free (new_priv->dirty_pspecs);
	new_priv->dirty_pspecs = NULL;
	if (new_priv->save_idle_id != 0)
	{
		g_source_remove (new_priv->save_idle_id);
		new_priv->save_idle_id = 0;
	}

	for (i = 0; i < n_pspecs; ++i)
	{
		GParamSpec *pspec = pspecs[i];

		if (pspec->owner_type != TERMINAL_TYPE_PROFILE ||
		        (pspec->flags & G_PARAM_WRITABLE) == 0)
			continue;

		new_priv->dirty_pspecs = g_slist_prepend (new_priv->dirty_pspecs, pspec);
	}
	g_free (pspecs);

	terminal_profile_save (new_profile);

	return new_profile;
}

/* Public API */

gboolean
terminal_profile_get_property_boolean (TerminalProfile *profile,
                                       const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_BOOLEAN (value), FALSE);
	if (!value || !G_VALUE_HOLDS_BOOLEAN (value))
		return FALSE;

	return g_value_get_boolean (value);
}

gconstpointer
terminal_profile_get_property_boxed (TerminalProfile *profile,
                                     const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_BOXED (value), NULL);
	if (!value || !G_VALUE_HOLDS_BOXED (value))
		return NULL;

	return g_value_get_boxed (value);
}

double
terminal_profile_get_property_double (TerminalProfile *profile,
                                      const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_DOUBLE (value), 0.0);
	if (!value || !G_VALUE_HOLDS_DOUBLE (value))
		return 0.0;

	return g_value_get_double (value);
}

int
terminal_profile_get_property_enum (TerminalProfile *profile,
                                    const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_ENUM (value), 0);
	if (!value || !G_VALUE_HOLDS_ENUM (value))
		return 0;

	return g_value_get_enum (value);
}

int
terminal_profile_get_property_int (TerminalProfile *profile,
                                   const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_INT (value), 0);
	if (!value || !G_VALUE_HOLDS_INT (value))
		return 0;

	return g_value_get_int (value);
}

gpointer
terminal_profile_get_property_object (TerminalProfile *profile,
                                      const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_OBJECT (value), NULL);
	if (!value || !G_VALUE_HOLDS_OBJECT (value))
		return NULL;

	return g_value_get_object (value);
}

const char*
terminal_profile_get_property_string (TerminalProfile *profile,
                                      const char *prop_name)
{
	const GValue *value;

	value = get_prop_value_from_prop_name (profile, prop_name);
	g_return_val_if_fail (value != NULL && G_VALUE_HOLDS_STRING (value), NULL);
	if (!value || !G_VALUE_HOLDS_STRING (value))
		return NULL;

	return g_value_get_string (value);
}

gboolean
terminal_profile_property_locked (TerminalProfile *profile,
                                  const char *prop_name)
{
	TerminalProfilePrivate *priv = profile->priv;
	GParamSpec *pspec;

	pspec = get_pspec_from_name (profile, prop_name);
	g_return_val_if_fail (pspec != NULL, FALSE);
	if (!pspec)
		return FALSE;

	return priv->locked[pspec->param_id];
}

void
terminal_profile_reset_property (TerminalProfile *profile,
                                 const char *prop_name)
{
	GParamSpec *pspec;

	pspec = get_pspec_from_name (profile, prop_name);
	g_return_if_fail (pspec != NULL);
	if (!pspec ||
	        (pspec->flags & G_PARAM_WRITABLE) == 0)
		return;

	terminal_profile_reset_property_internal (profile, pspec, TRUE);
}

gboolean
terminal_profile_get_palette (TerminalProfile *profile,
                              GdkRGBA *colors,
                              guint *n_colors)
{
	TerminalProfilePrivate *priv;
	GValueArray *array;
	guint i, n;

	g_return_val_if_fail (TERMINAL_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (colors != NULL && n_colors != NULL, FALSE);

	priv = profile->priv;
	array = g_value_get_boxed (g_value_array_get_nth (priv->properties, PROP_PALETTE));
	if (!array)
		return FALSE;

	n = MIN (array->n_values, *n_colors);
	for (i = 0; i < n; ++i)
	{
		GdkRGBA *color = g_value_get_boxed (g_value_array_get_nth (array, i));
		if (!color)
			continue; /* shouldn't happen!! */

		colors[i] = *color;
	}

	*n_colors = n;
	return TRUE;
}

gboolean
terminal_profile_get_palette_is_builtin (TerminalProfile *profile,
        guint *n)
{
	GdkRGBA colors[TERMINAL_PALETTE_SIZE];
	guint n_colors;
	guint i;

	n_colors = G_N_ELEMENTS (colors);
	if (!terminal_profile_get_palette (profile, colors, &n_colors) ||
	        n_colors != TERMINAL_PALETTE_SIZE)
		return FALSE;

	for (i = 0; i < TERMINAL_PALETTE_N_BUILTINS; ++i)
		if (palette_cmp (colors, terminal_palettes[i]))
		{
			*n = i;
			return TRUE;
		}

	return FALSE;
}

void
terminal_profile_set_palette_builtin (TerminalProfile *profile,
                                      guint n)
{
	GValue value = { 0, };

	g_return_if_fail (n < TERMINAL_PALETTE_N_BUILTINS);

	g_value_init (&value, G_TYPE_VALUE_ARRAY);
	set_value_from_palette (&value, terminal_palettes[n], TERMINAL_PALETTE_SIZE);
	g_object_set_property (G_OBJECT (profile), TERMINAL_PROFILE_PALETTE, &value);
	g_value_unset (&value);
}

gboolean
terminal_profile_modify_palette_entry (TerminalProfile *profile,
                                       guint            i,
                                       const GdkRGBA   *color)
{
	TerminalProfilePrivate *priv = profile->priv;
	GValueArray *array;
	GValue *value;
	GdkRGBA *old_color;

	array = g_value_get_boxed (g_value_array_get_nth (priv->properties, PROP_PALETTE));
	if (!array ||
	        i >= array->n_values)
		return FALSE;

	value = g_value_array_get_nth (array, i);
	old_color = g_value_get_boxed (value);
	if (!old_color ||
	        !rgba_equal (old_color, color))
	{
		g_value_set_boxed (value, color);
		g_object_notify (G_OBJECT (profile), TERMINAL_PROFILE_PALETTE);
	}

	return TRUE;
}
