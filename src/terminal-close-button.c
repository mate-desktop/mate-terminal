/*
 * terminal-close-button.c
 * Copyright (C) 2012-2021 MATE Developers
 *
 * Copyright © 2010 - Paolo Borelli
 * Copyright © 2011 - Ignacio Casal Quinteiro
 * Copyright © 2016 - Wolfgang Ulbrich
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

#include "terminal-close-button.h"

struct _TerminalCloseButtonClassPrivate {
	GtkCssProvider *css;
};

G_DEFINE_TYPE_WITH_CODE (TerminalCloseButton, terminal_close_button, GTK_TYPE_BUTTON,
                         g_type_add_class_private (g_define_type_id, sizeof (TerminalCloseButtonClassPrivate)))

static void
terminal_close_button_class_init (TerminalCloseButtonClass *klass)
{
	static const gchar button_style[] =
		"* {\n"
		  "padding: 0;\n"
		"}";

	klass->priv = G_TYPE_CLASS_GET_PRIVATE (klass, TERMINAL_TYPE_CLOSE_BUTTON, TerminalCloseButtonClassPrivate);

	klass->priv->css = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (klass->priv->css, button_style, -1, NULL);
}

static void
terminal_close_button_init (TerminalCloseButton *button)
{
	GtkWidget *image;
	GtkStyleContext *context;

	gtk_widget_set_name (GTK_WIDGET (button), "nate-terminal-tab-close-button");

	image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);

	gtk_container_add (GTK_CONTAINER (button), image);

	context = gtk_widget_get_style_context (GTK_WIDGET (button));
	gtk_style_context_add_provider (context,
	                                GTK_STYLE_PROVIDER (TERMINAL_CLOSE_BUTTON_GET_CLASS (button)->priv->css),
	                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

GtkWidget *
terminal_close_button_new ()
{
	return GTK_WIDGET (g_object_new (TERMINAL_TYPE_CLOSE_BUTTON,
	                                 "relief", GTK_RELIEF_NONE,
	                                 "focus-on-click", FALSE,
	                                 NULL));
}

