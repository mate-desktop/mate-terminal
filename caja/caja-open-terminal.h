/*
 *  caja-open-terminal.h
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
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Author: Christian Neumair <chris@mate-de.org>
 * 
 */

#ifndef CAJA_OPEN_TERMINAL_H
#define CAJA_OPEN_TERMINAL_H

#include <glib-object.h>

G_BEGIN_DECLS

/* Declarations for the open terminal extension object.  This object will be
 * instantiated by caja.  It implements the GInterfaces 
 * exported by libcaja. */


#define CAJA_TYPE_OPEN_TERMINAL	  (caja_open_terminal_get_type ())
#define CAJA_OPEN_TERMINAL(o)	  (G_TYPE_CHECK_INSTANCE_CAST ((o), CAJA_TYPE_OPEN_TERMINAL, CajaOpenTerminal))
#define CAJA_IS_OPEN_TERMINAL(o)	  (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAJA_TYPE_OPEN_TERMINAL))
typedef struct _CajaOpenTerminal      CajaOpenTerminal;
typedef struct _CajaOpenTerminalClass CajaOpenTerminalClass;

struct _CajaOpenTerminal {
	GObject parent_slot;
};

struct _CajaOpenTerminalClass {
	GObjectClass parent_slot;
};

GType caja_open_terminal_get_type      (void);
void  caja_open_terminal_register_type (GTypeModule *module);

G_END_DECLS

#endif
