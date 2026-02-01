/*
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

#include <string.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include "terminal-actions.h"
#include "terminal-app.h"
#include "terminal-intl.h"
#include "terminal-screen.h"
#include "terminal-util.h"
#include "terminal-window.h"

/*
 * Application-level action callbacks
 */

static void
action_quit (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GList *windows;

    /* Close all windows - this will trigger quit */
    windows = gtk_application_get_windows (app);
    while (windows)
    {
        GtkWidget *window = GTK_WIDGET (windows->data);
        windows = windows->next;
        gtk_widget_destroy (window);
    }
}

static void
action_new_window (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
    TerminalApp *app = TERMINAL_APP (user_data);
    TerminalWindow *window;
    TerminalProfile *profile;

    profile = terminal_app_get_profile_for_new_term (app);
    if (!profile)
        return;

    window = terminal_app_new_window (app, gdk_screen_get_default ());
    terminal_app_new_terminal (app, window, profile,
                               NULL, NULL, NULL, NULL, 1.0);
    gtk_window_present (GTK_WINDOW (window));
}

static void
action_preferences (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    TerminalApp *app = TERMINAL_APP (user_data);
    GtkWindow *parent = NULL;
    GList *windows;

    windows = gtk_application_get_windows (GTK_APPLICATION (app));
    if (windows)
        parent = GTK_WINDOW (windows->data);

    terminal_app_manage_profiles (app, parent);
}

static void
action_help (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    GtkApplication *app = GTK_APPLICATION (user_data);
    GtkWindow *parent = NULL;
    GList *windows;

    windows = gtk_application_get_windows (app);
    if (windows)
        parent = GTK_WINDOW (windows->data);

    terminal_util_show_help (NULL, parent);
}

/* Application action entries */
static const GActionEntry app_actions[] = {
    { TERMINAL_ACTION_QUIT, action_quit, NULL, NULL, NULL },
    { TERMINAL_ACTION_NEW_WINDOW, action_new_window, NULL, NULL, NULL },
    { TERMINAL_ACTION_PREFERENCES, action_preferences, NULL, NULL, NULL },
    { TERMINAL_ACTION_HELP, action_help, NULL, NULL, NULL },
};

/*
 * Window-level action callbacks
 */

static TerminalWindow *
get_terminal_window (gpointer user_data)
{
    if (TERMINAL_IS_WINDOW (user_data))
        return TERMINAL_WINDOW (user_data);
    return NULL;
}

static void
action_new_tab (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalApp *app = terminal_app_get ();
    TerminalProfile *profile = terminal_app_get_profile_for_new_term (app);

    if (profile)
    {
        TerminalScreen *screen = terminal_window_get_active (window);
        char *working_dir = NULL;

        if (screen)
            working_dir = terminal_screen_get_current_dir_with_fallback (screen);

        terminal_app_new_terminal (app, window, profile,
                                   NULL, NULL,
                                   working_dir,
                                   screen ? terminal_screen_get_initial_environment (screen) : NULL,
                                   1.0);
        g_free (working_dir);
    }
}

static void
action_close_tab (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        terminal_window_remove_screen (window, screen);
}

static void
action_close_window (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    gtk_widget_destroy (GTK_WIDGET (window));
}

static void
action_copy (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_copy_clipboard_format (VTE_TERMINAL (screen), VTE_FORMAT_TEXT);
}

static void
action_paste (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_paste_clipboard (VTE_TERMINAL (screen));
}

static void
action_select_all (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_select_all (VTE_TERMINAL (screen));
}

static void
action_fullscreen (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    GVariant *state = g_action_get_state (G_ACTION (action));
    gboolean active = g_variant_get_boolean (state);

    g_simple_action_set_state (action, g_variant_new_boolean (!active));

    if (!active)
        gtk_window_fullscreen (GTK_WINDOW (window));
    else
        gtk_window_unfullscreen (GTK_WINDOW (window));

    g_variant_unref (state);
}

static void
action_menubar (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    GVariant *state = g_action_get_state (G_ACTION (action));
    gboolean visible = g_variant_get_boolean (state);

    g_simple_action_set_state (action, g_variant_new_boolean (!visible));
    terminal_window_set_menubar_visible (window, !visible);

    g_variant_unref (state);
}

static void
action_prev_tab (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    GtkWidget *notebook = terminal_window_get_notebook (window);
    if (notebook)
    {
        int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
        if (page > 0)
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page - 1);
        else
        {
            int n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), n_pages - 1);
        }
    }
}

static void
action_next_tab (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    GtkWidget *notebook = terminal_window_get_notebook (window);
    if (notebook)
    {
        int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
        int n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
        if (page < n_pages - 1)
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page + 1);
        else
            gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
    }
}

static void
action_move_tab_left (GSimpleAction *action,
                      GVariant      *parameter,
                      gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    GtkWidget *notebook = terminal_window_get_notebook (window);
    if (notebook)
    {
        int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
        if (page > 0)
        {
            GtkWidget *child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);
            gtk_notebook_reorder_child (GTK_NOTEBOOK (notebook), child, page - 1);
        }
    }
}

static void
action_move_tab_right (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    GtkWidget *notebook = terminal_window_get_notebook (window);
    if (notebook)
    {
        int page = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));
        int n_pages = gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook));
        if (page < n_pages - 1)
        {
            GtkWidget *child = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page);
            gtk_notebook_reorder_child (GTK_NOTEBOOK (notebook), child, page + 1);
        }
    }
}

static void
action_find_next (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_search_find_next (VTE_TERMINAL (screen));
}

static void
action_find_prev (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_search_find_previous (VTE_TERMINAL (screen));
}

static void
action_reset (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_reset (VTE_TERMINAL (screen), TRUE, FALSE);
}

static void
action_reset_clear (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        vte_terminal_reset (VTE_TERMINAL (screen), TRUE, TRUE);
}

static void
action_new_profile (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    TerminalApp *app = terminal_app_get ();

    terminal_app_new_profile (app,
                              terminal_app_get_profile_for_new_term (app),
                              window ? GTK_WINDOW (window) : NULL);
}

static void
action_keybindings (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    TerminalApp *app = terminal_app_get ();

    terminal_app_edit_keybindings (app, window ? GTK_WINDOW (window) : NULL);
}

static void
action_current_profile (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalApp *app = terminal_app_get ();
    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
    {
        TerminalProfile *profile = terminal_screen_get_profile (screen);
        if (profile)
            terminal_app_edit_profile (app, profile, GTK_WINDOW (window), NULL);
    }
}

static void
action_zoom_in (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
    {
        double zoom = vte_terminal_get_font_scale (VTE_TERMINAL (screen));
        vte_terminal_set_font_scale (VTE_TERMINAL (screen), zoom * 1.1);
        terminal_window_update_size (window, screen, TRUE);
    }
}

static void
action_zoom_out (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
    {
        double zoom = vte_terminal_get_font_scale (VTE_TERMINAL (screen));
        vte_terminal_set_font_scale (VTE_TERMINAL (screen), zoom / 1.1);
        terminal_window_update_size (window, screen, TRUE);
    }
}

static void
action_zoom_normal (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
    {
        vte_terminal_set_font_scale (VTE_TERMINAL (screen), 1.0);
        terminal_window_update_size (window, screen, TRUE);
    }
}

static void
action_find (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    terminal_window_show_find_dialog (window);
}

static void
action_set_title (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    terminal_window_show_set_title_dialog (window);
}

static void
action_detach_tab (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    if (!window)
        return;

    TerminalScreen *screen = terminal_window_get_active (window);
    if (screen)
        terminal_window_detach_screen (window, screen);
}

static void
action_about (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
    TerminalWindow *window = get_terminal_window (user_data);
    GtkWindow *parent = window ? GTK_WINDOW (window) : NULL;

    terminal_window_show_about_dialog (parent);
}

/* Window action entries */
static const GActionEntry win_actions[] = {
    { TERMINAL_ACTION_NEW_TAB, action_new_tab, NULL, NULL, NULL },
    { TERMINAL_ACTION_NEW_PROFILE, action_new_profile, NULL, NULL, NULL },
    { TERMINAL_ACTION_CLOSE_TAB, action_close_tab, NULL, NULL, NULL },
    { TERMINAL_ACTION_CLOSE_WINDOW, action_close_window, NULL, NULL, NULL },
    { TERMINAL_ACTION_COPY, action_copy, NULL, NULL, NULL },
    { TERMINAL_ACTION_PASTE, action_paste, NULL, NULL, NULL },
    { TERMINAL_ACTION_SELECT_ALL, action_select_all, NULL, NULL, NULL },
    { TERMINAL_ACTION_KEYBINDINGS, action_keybindings, NULL, NULL, NULL },
    { TERMINAL_ACTION_CURRENT_PROFILE, action_current_profile, NULL, NULL, NULL },
    { TERMINAL_ACTION_ZOOM_IN, action_zoom_in, NULL, NULL, NULL },
    { TERMINAL_ACTION_ZOOM_OUT, action_zoom_out, NULL, NULL, NULL },
    { TERMINAL_ACTION_ZOOM_NORMAL, action_zoom_normal, NULL, NULL, NULL },
    { TERMINAL_ACTION_FULLSCREEN, action_fullscreen, NULL, "false", NULL },
    { TERMINAL_ACTION_MENUBAR, action_menubar, NULL, "true", NULL },
    { TERMINAL_ACTION_FIND, action_find, NULL, NULL, NULL },
    { TERMINAL_ACTION_FIND_NEXT, action_find_next, NULL, NULL, NULL },
    { TERMINAL_ACTION_FIND_PREV, action_find_prev, NULL, NULL, NULL },
    { TERMINAL_ACTION_SET_TITLE, action_set_title, NULL, NULL, NULL },
    { TERMINAL_ACTION_RESET, action_reset, NULL, NULL, NULL },
    { TERMINAL_ACTION_RESET_CLEAR, action_reset_clear, NULL, NULL, NULL },
    { TERMINAL_ACTION_PREV_TAB, action_prev_tab, NULL, NULL, NULL },
    { TERMINAL_ACTION_NEXT_TAB, action_next_tab, NULL, NULL, NULL },
    { TERMINAL_ACTION_MOVE_TAB_LEFT, action_move_tab_left, NULL, NULL, NULL },
    { TERMINAL_ACTION_MOVE_TAB_RIGHT, action_move_tab_right, NULL, NULL, NULL },
    { TERMINAL_ACTION_DETACH_TAB, action_detach_tab, NULL, NULL, NULL },
    { "about", action_about, NULL, NULL, NULL },
};

/*
 * Public API
 */

void
terminal_actions_init_app (GtkApplication *app)
{
    g_return_if_fail (GTK_IS_APPLICATION (app));

    g_action_map_add_action_entries (G_ACTION_MAP (app),
                                     app_actions,
                                     G_N_ELEMENTS (app_actions),
                                     app);
}

void
terminal_actions_init_window (GtkApplicationWindow *window)
{
    g_return_if_fail (GTK_IS_APPLICATION_WINDOW (window));

    g_action_map_add_action_entries (G_ACTION_MAP (window),
                                     win_actions,
                                     G_N_ELEMENTS (win_actions),
                                     window);
}
