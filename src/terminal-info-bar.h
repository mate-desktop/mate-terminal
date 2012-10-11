/*
 *  Copyright © 2010 Christian Persch
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope info_bar it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef TERMINAL_INFO_BAR_H
#define TERMINAL_INFO_BAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define TERMINAL_TYPE_INFO_BAR         (terminal_info_bar_get_type ())
#define TERMINAL_INFO_BAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_INFO_BAR, TerminalInfoBar))
#define TERMINAL_INFO_BAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_INFO_BAR, TerminalInfoBarClass))
#define TERMINAL_IS_INFO_BAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_INFO_BAR))
#define TERMINAL_IS_INFO_BAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_INFO_BAR))
#define TERMINAL_INFO_BAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_INFO_BAR, TerminalInfoBarClass))

typedef struct _TerminalInfoBar        TerminalInfoBar;
typedef struct _TerminalInfoBarClass   TerminalInfoBarClass;
typedef struct _TerminalInfoBarPrivate TerminalInfoBarPrivate;

struct _TerminalInfoBar
{
	GtkInfoBar parent_instance;

	/*< private >*/
	TerminalInfoBarPrivate *priv;
};

struct _TerminalInfoBarClass
{
	GtkInfoBarClass parent_class;
};

GType terminal_info_bar_get_type (void);

GtkWidget *terminal_info_bar_new (GtkMessageType type,
                                  const char *first_button_text,
                                  ...) G_GNUC_NULL_TERMINATED;

void terminal_info_bar_format_text (TerminalInfoBar *bar,
                                    const char *format,
                                    ...) G_GNUC_PRINTF (2, 3);

G_END_DECLS

#endif /* !TERMINAL_INFO_BAR_H */
