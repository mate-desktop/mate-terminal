/*
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef TERMINAL_ACTIONS_H
#define TERMINAL_ACTIONS_H

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

/* Initialize application-level actions */
void terminal_actions_init_app (GtkApplication *app);

/* Initialize window-level actions */
void terminal_actions_init_window (GtkApplicationWindow *window);

/* Action name definitions for accelerator binding */

/* Application-level actions (prefix: app.) */
#define TERMINAL_ACTION_QUIT           "quit"
#define TERMINAL_ACTION_NEW_WINDOW     "new-window"
#define TERMINAL_ACTION_PREFERENCES    "preferences"
#define TERMINAL_ACTION_HELP           "help"
#define TERMINAL_ACTION_ABOUT          "about"

/* Window-level actions (prefix: win.) */
#define TERMINAL_ACTION_NEW_TAB        "new-tab"
#define TERMINAL_ACTION_NEW_PROFILE    "new-profile"
#define TERMINAL_ACTION_SAVE_CONTENTS  "save-contents"
#define TERMINAL_ACTION_CLOSE_TAB      "close-tab"
#define TERMINAL_ACTION_CLOSE_WINDOW   "close-window"

#define TERMINAL_ACTION_COPY           "copy"
#define TERMINAL_ACTION_PASTE          "paste"
#define TERMINAL_ACTION_PASTE_URIS     "paste-uris"
#define TERMINAL_ACTION_SELECT_ALL     "select-all"
#define TERMINAL_ACTION_PROFILES       "profiles"
#define TERMINAL_ACTION_KEYBINDINGS    "keybindings"
#define TERMINAL_ACTION_CURRENT_PROFILE "current-profile"

#define TERMINAL_ACTION_ZOOM_IN        "zoom-in"
#define TERMINAL_ACTION_ZOOM_OUT       "zoom-out"
#define TERMINAL_ACTION_ZOOM_NORMAL    "zoom-normal"
#define TERMINAL_ACTION_FULLSCREEN     "fullscreen"
#define TERMINAL_ACTION_MENUBAR        "menubar"

#define TERMINAL_ACTION_FIND           "find"
#define TERMINAL_ACTION_FIND_NEXT      "find-next"
#define TERMINAL_ACTION_FIND_PREV      "find-prev"
#define TERMINAL_ACTION_FIND_CLEAR     "find-clear"

#define TERMINAL_ACTION_SET_TITLE      "set-title"
#define TERMINAL_ACTION_RESET          "reset"
#define TERMINAL_ACTION_RESET_CLEAR    "reset-clear"
#define TERMINAL_ACTION_ADD_ENCODING   "add-encoding"

#define TERMINAL_ACTION_PREV_TAB       "prev-tab"
#define TERMINAL_ACTION_NEXT_TAB       "next-tab"
#define TERMINAL_ACTION_MOVE_TAB_LEFT  "move-tab-left"
#define TERMINAL_ACTION_MOVE_TAB_RIGHT "move-tab-right"
#define TERMINAL_ACTION_DETACH_TAB     "detach-tab"

#define TERMINAL_ACTION_PREV_PROFILE   "prev-profile"
#define TERMINAL_ACTION_NEXT_PROFILE   "next-profile"

G_END_DECLS

#endif /* TERMINAL_ACTIONS_H */
