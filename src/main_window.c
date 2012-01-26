/*
 *  Copyright (c) Stephan Arts 2006-2011 <stephan@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software 
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */ 

#include <config.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <string.h>

#include <gio/gio.h>

#include <exo/exo.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libexif/exif-data.h>

#include <dbus/dbus-glib.h>

#include <cairo/cairo.h>

#include "settings.h"
#include "util.h"
#include "file.h"
#include "image_list.h"
#include "image_viewer.h"
#include "main_window.h"
#include "main_window_ui.h"
#include "thumbnail_bar.h"
#include "wallpaper_manager.h"

#include "xfce_wallpaper_manager.h"
#include "gnome_wallpaper_manager.h"

#include "privacy_dialog.h"
#include "properties_dialog.h"
#include "preferences_dialog.h"
#include "app_menu_item.h"

#ifndef RISTRETTO_APP_TITLE
#define RISTRETTO_APP_TITLE _("Image Viewer")
#endif

#ifndef RISTRETTO_HELP_LOCATION
#define RISTRETTO_HELP_LOCATION "file://"DOCDIR"/html/C/index.html"
#endif


#define RSTTO_RECENT_FILES_APP_NAME "ristretto"
#define RSTTO_RECENT_FILES_GROUP "Graphics"

struct _RsttoMainWindowPriv
{
    RsttoImageList *image_list;

    DBusGConnection *connection;
    DBusGProxy *filemanager_proxy;

    guint show_fs_toolbar_timeout_id;
    gint window_save_geometry_timer_id;
    
    gboolean fs_toolbar_sticky;

    RsttoImageListIter *iter;

    GtkActionGroup   *action_group;
    GtkUIManager     *ui_manager;
    GtkRecentManager *recent_manager;
    RsttoSettings    *settings_manager;
    RsttoWallpaperManager *wallpaper_manager;

    GtkWidget *menubar;
    GtkWidget *toolbar;
    GtkWidget *image_list_toolbar;
    GtkWidget *image_viewer_menu;
    GtkWidget *position_menu;
    GtkWidget *image_viewer;
    GtkWidget *p_viewer_s_window;
    GtkWidget *table;
    GtkWidget *hpaned_left;
    GtkWidget *hpaned_right;
    GtkWidget *vpaned_top;
    GtkWidget *vpaned_bottom;
    GtkWidget *thumbnailbar;
    GtkWidget *statusbar;
    guint statusbar_context_id;

    GtkWidget *back;
    GtkWidget *forward;

    guint      t_open_merge_id;
    guint      recent_merge_id;
    guint      play_merge_id;
    guint      pause_merge_id;
    guint      toolbar_play_merge_id;
    guint      toolbar_pause_merge_id;
    guint      toolbar_fullscreen_merge_id;
    guint      toolbar_unfullscreen_merge_id;

    GtkAction *play_action;
    GtkAction *pause_action;
    GtkAction *recent_action;

    gboolean playing;
    gint play_timeout_id;

    GtkFileFilter *filter;
};

enum
{
    PROP_0,
    PROP_IMAGE_LIST,
};

static void
rstto_main_window_init (RsttoMainWindow *);
static void
rstto_main_window_class_init(RsttoMainWindowClass *);
static void
rstto_main_window_dispose(GObject *object);
static void
rstto_main_window_size_allocate (GtkWidget *, GtkAllocation *);


static gboolean
key_press_event (
        GtkWidget *widget,
        GdkEventKey *event);

static gboolean
rstto_window_save_geometry_timer (gpointer user_data);

static void
rstto_main_window_image_list_iter_changed (RsttoMainWindow *window);

static gboolean
cb_rstto_main_window_configure_event (GtkWidget *widget, GdkEventConfigure *event);
static void
cb_rstto_main_window_state_event(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data);
static gboolean
cb_rstto_main_window_show_fs_toolbar_timeout (RsttoMainWindow *window);
static void
cb_rstto_main_window_image_list_iter_changed (RsttoImageListIter *iter, RsttoMainWindow *window);
static void
rstto_main_window_update_statusbar (RsttoMainWindow *window);

static void
cb_rstto_main_window_zoom_100 (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_zoom_fit (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_zoom_in (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_zoom_out (GtkWidget *widget, RsttoMainWindow *window);

static void
cb_rstto_main_window_rotate_cw (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_rotate_ccw (GtkWidget *widget, RsttoMainWindow *window);

static void
cb_rstto_main_window_next_image (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_previous_image (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_first_image (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_last_image (GtkWidget *widget, RsttoMainWindow *window);

static void
cb_rstto_main_window_open_image (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_open_recent(GtkRecentChooser *chooser, RsttoMainWindow *window);
static void
cb_rstto_main_window_properties (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_close (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_save_copy (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_delete (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_dnd_files (GtkWidget *widget, gchar **uris, RsttoMainWindow *window);

static void
cb_rstto_main_window_set_as_wallpaper (GtkWidget *widget, RsttoMainWindow *window);
static void
cb_rstto_main_window_sorting_function_changed (GtkRadioAction *action, GtkRadioAction *current,  RsttoMainWindow *window);
static void
cb_rstto_main_window_navigationtoolbar_position_changed (GtkRadioAction *, GtkRadioAction *,  RsttoMainWindow *window);
static void
cb_rstto_main_window_navigationtoolbar_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static void
cb_rstto_main_window_update_statusbar (GtkWidget *widget, RsttoMainWindow *window);

static void
cb_rstto_main_window_play (
        GtkWidget *widget,
        RsttoMainWindow *window);
static void
cb_rstto_main_window_pause(
        GtkWidget *widget,
        RsttoMainWindow *window);
static gboolean
cb_rstto_main_window_play_slideshow (
        RsttoMainWindow *window);

static void
cb_rstto_main_window_toggle_show_file_toolbar (
        GtkWidget *widget,
        RsttoMainWindow *window);
static void
cb_rstto_main_window_toggle_show_nav_toolbar (
        GtkWidget *widget,
        RsttoMainWindow *window);
static void
cb_rstto_main_window_toggle_show_thumbnailbar (
        GtkWidget *widget,
        RsttoMainWindow *window);

static void
cb_rstto_main_window_fullscreen (
        GtkWidget *widget,
        RsttoMainWindow *window);
static void
cb_rstto_main_window_preferences (
        GtkWidget *widget,
        RsttoMainWindow *window);

static void
cb_rstto_main_window_clear_private_data (
        GtkWidget *widget,
        RsttoMainWindow *window);

static void
cb_rstto_main_window_about (
        GtkWidget *widget,
        RsttoMainWindow *window);

static void
cb_rstto_main_window_contents (
        GtkWidget *widget,
        RsttoMainWindow *window);

static void
cb_rstto_main_window_quit (
        GtkWidget *widget,
        RsttoMainWindow *window);

static gboolean 
cb_rstto_main_window_motion_notify_event (
        RsttoMainWindow *window,
        GdkEventMotion *event,
        gpointer user_data);

static gboolean
cb_rstto_main_window_image_viewer_enter_notify_event (
        GtkWidget *widget,
        GdkEventCrossing *event,
        gpointer user_data);

static gboolean
cb_rstto_main_window_image_viewer_scroll_event (
        GtkWidget *widget,
        GdkEventScroll *event,
        gpointer user_data);

static void
rstto_main_window_update_buttons (
        RsttoMainWindow *window);

static void
rstto_main_window_set_navigationbar_position (
        RsttoMainWindow *window,
        guint orientation);


static void
cb_rstto_merge_toolbars_changed (
        GObject *settings,
        GParamSpec *pspec,
        gpointer user_data);
static void
cb_rstto_wrap_images_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data);
static void
cb_rstto_desktop_type_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data);



static GtkWidgetClass *parent_class = NULL;

static GtkActionEntry action_entries[] =
{
/* File Menu */
  { "file-menu", NULL, N_ ("_File"), NULL, },
  { "open", "document-open", N_ ("_Open"), "<control>O", N_ ("Open an image"), G_CALLBACK (cb_rstto_main_window_open_image), },
  { "save-copy", GTK_STOCK_SAVE_AS, N_ ("_Save copy"), "<control>s", N_ ("Save a copy of the image"), G_CALLBACK (cb_rstto_main_window_save_copy), },
  { "properties", GTK_STOCK_PROPERTIES, N_ ("_Properties"), NULL, N_ ("Show file properties"), G_CALLBACK (cb_rstto_main_window_properties), },
  { "close", GTK_STOCK_CLOSE, N_ ("_Close"), "<control>W", N_ ("Close this image"), G_CALLBACK (cb_rstto_main_window_close), },
  { "quit", GTK_STOCK_QUIT, N_ ("_Quit"), "<control>Q", N_ ("Quit Ristretto"), G_CALLBACK (cb_rstto_main_window_quit), },
/* Edit Menu */
  { "edit-menu", NULL, N_ ("_Edit"), NULL, },
  { "open-with-menu", NULL, N_ ("_Open with..."), NULL, },
  { "sorting-menu", NULL, N_ ("_Sorting"), NULL, },
  { "delete", GTK_STOCK_DELETE, N_ ("_Delete"), "Delete", N_ ("Delete this image from disk"), G_CALLBACK (cb_rstto_main_window_delete), },
  { "clear-private-data", GTK_STOCK_CLEAR, N_ ("_Clear private data"), "<control><shift>Delete", NULL, G_CALLBACK(cb_rstto_main_window_clear_private_data), },
  { "preferences", GTK_STOCK_PREFERENCES, N_ ("_Preferences"), NULL, NULL, G_CALLBACK (cb_rstto_main_window_preferences), },
/* View Menu */
  { "view-menu", NULL, N_ ("_View"), NULL, },
  { "fullscreen", GTK_STOCK_FULLSCREEN, N_ ("_Fullscreen"), "F11", NULL, G_CALLBACK (cb_rstto_main_window_fullscreen), },
  { "unfullscreen", GTK_STOCK_LEAVE_FULLSCREEN, N_ ("_Leave Fullscreen"), NULL, NULL, G_CALLBACK (cb_rstto_main_window_fullscreen), },
  { "set-as-wallpaper", "preferences-desktop-wallpaper", N_ ("_Set as Wallpaper"), NULL, NULL, G_CALLBACK (cb_rstto_main_window_set_as_wallpaper), },
/* Zoom submenu */
  { "zoom-menu", NULL, N_ ("_Zoom"), NULL, },
  { "zoom-in", GTK_STOCK_ZOOM_IN, N_ ("Zoom _In"), "<control>plus", NULL, G_CALLBACK (cb_rstto_main_window_zoom_in),},
  { "zoom-out", GTK_STOCK_ZOOM_OUT, N_ ("Zoom _Out"), "<control>minus", NULL, G_CALLBACK (cb_rstto_main_window_zoom_out), },
  { "zoom-fit", GTK_STOCK_ZOOM_FIT, N_ ("Zoom _Fit"), "<control>equal", NULL, G_CALLBACK (cb_rstto_main_window_zoom_fit), },
  { "zoom-100", GTK_STOCK_ZOOM_100, N_ ("_Normal Size"), "<control>0", NULL, G_CALLBACK (cb_rstto_main_window_zoom_100), },
/* Rotation submenu */
  { "rotation-menu", NULL, N_ ("_Rotation"), NULL, },
  { "rotate-cw", "object-rotate-right", N_ ("Rotate _Right"), "<control>bracketright", NULL, G_CALLBACK (cb_rstto_main_window_rotate_cw), },
  { "rotate-ccw", "object-rotate-left", N_ ("Rotate _Left"), "<control>bracketleft", NULL, G_CALLBACK (cb_rstto_main_window_rotate_ccw), },
/* Go Menu */
  { "go-menu",  NULL, N_ ("_Go"), NULL, },
  { "forward",  GTK_STOCK_GO_FORWARD, N_ ("_Forward"), "space", NULL, G_CALLBACK (cb_rstto_main_window_next_image), },
  { "back",     GTK_STOCK_GO_BACK, N_ ("_Back"), "BackSpace", NULL, G_CALLBACK (cb_rstto_main_window_previous_image), },
  { "first",    GTK_STOCK_GOTO_FIRST, N_ ("_First"), "Home", NULL, G_CALLBACK (cb_rstto_main_window_first_image), },
  { "last",     GTK_STOCK_GOTO_LAST, N_ ("_Last"), "End", NULL, G_CALLBACK (cb_rstto_main_window_last_image), },
/* Help Menu */
  { "help-menu", NULL, N_ ("_Help"), NULL, },
  { "contents", GTK_STOCK_HELP,
                N_ ("_Contents"),
                "F1",
                N_ ("Display ristretto user manual"),
                G_CALLBACK (cb_rstto_main_window_contents), },
  { "about",    GTK_STOCK_ABOUT, 
                N_ ("_About"),
                NULL,
                N_ ("Display information about ristretto"),
                G_CALLBACK (cb_rstto_main_window_about), },
/* Position Menu */
  { "position-menu", NULL, N_ ("_Position"), NULL, },
  { "thumbnailbar-position-menu", NULL, N_ ("Thumbnail Bar _Position"), NULL, },
/* Misc */
  { "leave-fullscreen", GTK_STOCK_LEAVE_FULLSCREEN, N_ ("Leave _Fullscreen"), NULL, NULL, G_CALLBACK (cb_rstto_main_window_fullscreen), },
  { "tb-menu", NULL, NULL, NULL, }
};

/** Toggle Action Entries */
static const GtkToggleActionEntry toggle_action_entries[] =
{
    /* Toggle visibility of the main file toolbar */
    { "show-file-toolbar", NULL, N_ ("Show _File Toolbar"), NULL, NULL, G_CALLBACK (cb_rstto_main_window_toggle_show_file_toolbar), TRUE, },
    /* Toggle visibility of the main navigation toolbar */
    { "show-nav-toolbar", NULL, N_ ("Show _Navigation Toolbar"), NULL, NULL, G_CALLBACK (cb_rstto_main_window_toggle_show_nav_toolbar), TRUE, },
    /* Toggle visibility of the thumbnailbar*/
    { "show-thumbnailbar", NULL, N_ ("Show _Thumbnail Bar"), "<control>M", NULL, G_CALLBACK (cb_rstto_main_window_toggle_show_thumbnailbar), TRUE, },
};

/** Image sorting options*/
static const GtkRadioActionEntry radio_action_sort_entries[] = 
{
    /* Sort by Filename */
    {"sort-filename", NULL, N_("sort by filename"), NULL, NULL, 0},
    /* Sort by Date*/
    {"sort-date", NULL, N_("sort by date"), NULL, NULL, 1},
};

/** Navigationbar+Thumbnailbar positioning options*/
static const GtkRadioActionEntry radio_action_pos_entries[] = 
{
    { "pos-left", NULL, N_("Left"), NULL, NULL, 0},
    { "pos-right", NULL, N_("Right"), NULL, NULL, 1},
    { "pos-top", NULL, N_("Top"), NULL, NULL, 2},
    { "pos-bottom", NULL, N_("Bottom"), NULL, NULL, 3},
};


GType
rstto_main_window_get_type (void)
{
    static GType rstto_main_window_type = 0;

    if (!rstto_main_window_type)
    {
        static const GTypeInfo rstto_main_window_info = 
        {
            sizeof (RsttoMainWindowClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) rstto_main_window_class_init,
            (GClassFinalizeFunc) NULL,
            NULL,
            sizeof (RsttoMainWindow),
            0,
            (GInstanceInitFunc) rstto_main_window_init,
            NULL
        };

        rstto_main_window_type = g_type_register_static (GTK_TYPE_WINDOW, "RsttoMainWindow", &rstto_main_window_info, 0);

    }



    return rstto_main_window_type;
}

static void
rstto_main_window_init (RsttoMainWindow *window)
{
    GtkAccelGroup   *accel_group;
    GtkWidget       *separator;
    GtkWidget       *main_vbox = gtk_vbox_new (FALSE, 0);
    GtkRecentFilter *recent_filter;
    guint            window_width, window_height;
    gchar           *desktop_type = NULL;

    GClosure        *toggle_fullscreen_closure = g_cclosure_new ((GCallback)cb_rstto_main_window_fullscreen, window, NULL);
    GClosure        *leave_fullscreen_closure = g_cclosure_new_swap ((GCallback)gtk_window_unfullscreen, window, NULL);
    GClosure        *next_image_closure = g_cclosure_new ((GCallback)cb_rstto_main_window_next_image, window, NULL);
    GClosure        *previous_image_closure = g_cclosure_new ((GCallback)cb_rstto_main_window_previous_image, window, NULL);
    GClosure        *quit_closure = g_cclosure_new ((GCallback)cb_rstto_main_window_quit, window, NULL);

    guint navigationbar_position = 3;

    gtk_window_set_title (GTK_WINDOW (window), RISTRETTO_APP_TITLE);

    window->priv = g_new0(RsttoMainWindowPriv, 1);
    

    window->priv->iter = NULL;

    window->priv->ui_manager = gtk_ui_manager_new ();
    window->priv->recent_manager = gtk_recent_manager_get_default();
    window->priv->settings_manager = rstto_settings_new();

    /* Setup the image filter list for drag and drop */
    window->priv->filter = gtk_file_filter_new ();
    g_object_ref_sink (window->priv->filter);
    gtk_file_filter_add_pixbuf_formats (window->priv->filter);

    /* D-Bus stuff */

    window->priv->connection = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
    if (window->priv->connection)
    {
        window->priv->filemanager_proxy =
                dbus_g_proxy_new_for_name(
                        window->priv->connection,
                        "org.xfce.FileManager",
                        "/org/xfce/FileManager",
                        "org.xfce.FileManager");
    }

    desktop_type = rstto_settings_get_string_property (window->priv->settings_manager, "desktop-type");
    if (desktop_type)
    {
        if (!g_strcasecmp(desktop_type, "xfce"))
        {
            window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new();
        }

        if (!g_strcasecmp(desktop_type, "gnome"))
        {
            window->priv->wallpaper_manager = rstto_gnome_wallpaper_manager_new();
        }

        if (!g_strcasecmp(desktop_type, "none"))
        {
            window->priv->wallpaper_manager = NULL;
        }

        g_free (desktop_type);
        desktop_type = NULL;
    }
    else
    {
        /* Default to xfce */
        window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new();
    }


    navigationbar_position = rstto_settings_get_navbar_position (window->priv->settings_manager);

    accel_group = gtk_ui_manager_get_accel_group (window->priv->ui_manager);
    gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);

    gtk_accel_group_connect_by_path (accel_group, "<Window>/fullscreen", toggle_fullscreen_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/unfullscreen", leave_fullscreen_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/next-image", next_image_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/previous-image", previous_image_closure);
    gtk_accel_group_connect_by_path (accel_group, "<Window>/quit", quit_closure);

    /* Set default accelerators */
    gtk_accel_map_change_entry ("<Window>/fullscreen", GDK_F, 0, FALSE);
    gtk_accel_map_change_entry ("<Window>/unfullscreen", GDK_Escape, 0, FALSE);
    gtk_accel_map_change_entry ("<Window>/next-image", GDK_Page_Down, 0, FALSE);
    gtk_accel_map_change_entry ("<Window>/previous-image", GDK_Page_Up, 0, FALSE);
    gtk_accel_map_change_entry ("<Window>/quit", GDK_q, 0, FALSE);
    if (gtk_accel_map_lookup_entry ("<Actions>/RsttoWindow/play", NULL) == FALSE)
    {
        gtk_accel_map_change_entry ("<Actions>/RsttoWindow/play", GDK_F5, 0, FALSE);
    }

    /* Create mergeid's for adding ui-components */
    window->priv->recent_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->play_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->pause_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_play_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_pause_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_fullscreen_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);
    window->priv->toolbar_unfullscreen_merge_id = gtk_ui_manager_new_merge_id (window->priv->ui_manager);


    /* Create Play/Pause Slideshow actions */
    window->priv->play_action = gtk_action_new ("play", _("_Play"), _("Play slideshow"), GTK_STOCK_MEDIA_PLAY);
    window->priv->pause_action = gtk_action_new ("pause", _("_Pause"), _("Pause slideshow"), GTK_STOCK_MEDIA_PAUSE);

    /* Create Recently used items Action */
    window->priv->recent_action = gtk_recent_action_new_for_manager ("document-open-recent", _("_Recently used"), _("Recently used"), 0, GTK_RECENT_MANAGER(window->priv->recent_manager));

    gtk_recent_chooser_set_sort_type (GTK_RECENT_CHOOSER (window->priv->recent_action), GTK_RECENT_SORT_MRU);

    /**
     * Add a filter to the recent-chooser
     */
    recent_filter = gtk_recent_filter_new();
    gtk_recent_filter_add_application (recent_filter, "ristretto");
    gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(window->priv->recent_action), recent_filter);

    /* Add the same accelerator path to play and pause, so the same kb-shortcut will be used for starting and stopping the slideshow */
    gtk_action_set_accel_path (window->priv->pause_action, "<Actions>/RsttoWindow/play");
    gtk_action_set_accel_path (window->priv->play_action, "<Actions>/RsttoWindow/play");

    /* Add the play and pause actions to the actiongroup */
    window->priv->action_group = gtk_action_group_new ("RsttoWindow");
    gtk_action_group_add_action (window->priv->action_group,
                                 window->priv->play_action);
    gtk_action_group_add_action (window->priv->action_group,
                                 window->priv->pause_action);
    gtk_action_group_add_action (window->priv->action_group,
                                 window->priv->recent_action);
    /* Connect signal-handlers */
    g_signal_connect(G_OBJECT(window->priv->play_action), "activate", G_CALLBACK(cb_rstto_main_window_play), window);
    g_signal_connect(G_OBJECT(window->priv->pause_action), "activate", G_CALLBACK(cb_rstto_main_window_pause), window);
    g_signal_connect(G_OBJECT(window->priv->recent_action), "item-activated", G_CALLBACK(cb_rstto_main_window_open_recent), window);

    gtk_ui_manager_insert_action_group (window->priv->ui_manager, window->priv->action_group, 0);

    gtk_action_group_set_translation_domain (window->priv->action_group, GETTEXT_PACKAGE);
    gtk_action_group_add_actions (window->priv->action_group, action_entries, G_N_ELEMENTS (action_entries), GTK_WIDGET (window));
    gtk_action_group_add_toggle_actions (window->priv->action_group, toggle_action_entries, G_N_ELEMENTS (toggle_action_entries), GTK_WIDGET (window));
    gtk_action_group_add_radio_actions (window->priv->action_group, radio_action_sort_entries , G_N_ELEMENTS (radio_action_sort_entries), 0, G_CALLBACK (cb_rstto_main_window_sorting_function_changed), GTK_WIDGET (window));
    gtk_action_group_add_radio_actions (window->priv->action_group, radio_action_pos_entries, G_N_ELEMENTS (radio_action_pos_entries), navigationbar_position, G_CALLBACK (cb_rstto_main_window_navigationtoolbar_position_changed), GTK_WIDGET (window));


    gtk_ui_manager_add_ui_from_string (window->priv->ui_manager,main_window_ui, main_window_ui_length, NULL);
    window->priv->menubar = gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu");
    window->priv->toolbar = gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar");
    window->priv->image_list_toolbar = gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar");
    window->priv->image_viewer_menu = gtk_ui_manager_get_widget (window->priv->ui_manager, "/image-viewer-menu");
    window->priv->position_menu = gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar-menu");

    /**
     * Get the separator toolitem and tell it to expand
     */
    separator = gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/separator-1");
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (separator), TRUE);
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);

    separator = gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/separator-1");
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (separator), TRUE);
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);

    separator = gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/separator-2");
    gtk_tool_item_set_expand (GTK_TOOL_ITEM (separator), TRUE);
    gtk_separator_tool_item_set_draw (GTK_SEPARATOR_TOOL_ITEM (separator), FALSE);

    /**
     * Make the back and forward toolitems important,
     * when they are, the labels are shown when the toolbar style is 'both-horizontal'
     */
    window->priv->back = gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/back");
    window->priv->forward = gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/forward");
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (window->priv->back), TRUE);
    gtk_tool_item_set_is_important (GTK_TOOL_ITEM (window->priv->forward), TRUE);
    
    window->priv->image_viewer = rstto_image_viewer_new ();
    window->priv->p_viewer_s_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window->priv->p_viewer_s_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add (GTK_CONTAINER (window->priv->p_viewer_s_window), window->priv->image_viewer);

    rstto_image_viewer_set_menu (
        RSTTO_IMAGE_VIEWER(window->priv->image_viewer),
        GTK_MENU(window->priv->image_viewer_menu));

    window->priv->thumbnailbar = rstto_thumbnail_bar_new (NULL);

    window->priv->hpaned_left = gtk_hpaned_new();
    window->priv->hpaned_right = gtk_hpaned_new();
    window->priv->vpaned_top = gtk_vpaned_new();
    window->priv->vpaned_bottom = gtk_vpaned_new();
    window->priv->table = gtk_table_new (3, 3, FALSE);

    gtk_paned_pack2 (GTK_PANED (window->priv->hpaned_left), window->priv->hpaned_right, TRUE, FALSE);
    gtk_paned_pack1 (GTK_PANED (window->priv->hpaned_right), window->priv->vpaned_top, TRUE, FALSE);
    gtk_paned_pack2 (GTK_PANED (window->priv->vpaned_top), window->priv->vpaned_bottom, TRUE, FALSE);

    gtk_paned_pack1 (GTK_PANED (window->priv->vpaned_bottom), window->priv->p_viewer_s_window, TRUE, FALSE);
    gtk_paned_pack2 (GTK_PANED (window->priv->hpaned_right), window->priv->thumbnailbar, FALSE, FALSE);


    window->priv->statusbar = gtk_statusbar_new();
    window->priv->statusbar_context_id = gtk_statusbar_get_context_id (GTK_STATUSBAR(window->priv->statusbar), "image-data");
    gtk_statusbar_push (GTK_STATUSBAR(window->priv->statusbar), 
                        gtk_statusbar_get_context_id (GTK_STATUSBAR(window->priv->statusbar), "fallback-data"),
                        _("Press open to select an image"));


    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    gtk_box_pack_start(GTK_BOX(main_vbox), window->priv->menubar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), window->priv->toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), window->priv->table, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), window->priv->statusbar, FALSE, FALSE, 0);

    gtk_table_attach_defaults (GTK_TABLE (window->priv->table), window->priv->hpaned_left, 1, 2, 1, 2);
    gtk_table_attach (GTK_TABLE (window->priv->table), window->priv->image_list_toolbar, 0, 1, 0, 3, GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);

    gtk_widget_set_no_show_all (window->priv->toolbar, TRUE);
    gtk_widget_set_no_show_all (window->priv->image_list_toolbar, TRUE);
    gtk_widget_set_no_show_all (window->priv->thumbnailbar, TRUE);
    gtk_widget_set_no_show_all ( gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-nav-toolbar"), TRUE);

    rstto_main_window_set_navigationbar_position (window, navigationbar_position);

    /**
     * Add missing pieces to the UI
     */
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->play_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "play",
                           "play",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->recent_merge_id,
                           "/main-menu/file-menu/placeholder-open-recent",
                           "document-open-recent",
                           "document-open-recent",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->toolbar_play_merge_id,
                           "/navigation-toolbar/placeholder-slideshow",
                           "play",
                           "play",
                           GTK_UI_MANAGER_TOOLITEM,
                           FALSE);
    /**
     * Retrieve the last window-size from the settings-manager
     * and make it the default for this window
     */
    window_width = rstto_settings_get_uint_property (RSTTO_SETTINGS (window->priv->settings_manager), "window-width");
    window_height = rstto_settings_get_uint_property (RSTTO_SETTINGS (window->priv->settings_manager), "window-height");
    gtk_window_set_default_size(GTK_WINDOW(window), window_width, window_height);

    /**
     * Retrieve the toolbar state from the settings-manager
     */
    if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-file-toolbar"))
    {
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-file-toolbar")),
                TRUE);
        gtk_widget_show (window->priv->toolbar);
    }
    else
    {
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-file-toolbar")),
                FALSE);
        gtk_widget_hide (window->priv->toolbar);
    }

    if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-nav-toolbar"))
    {
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-nav-toolbar")),
                TRUE);
        gtk_widget_show (window->priv->image_list_toolbar);
    }
    else
    {
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-nav-toolbar")),
                FALSE);
        gtk_widget_hide (window->priv->image_list_toolbar);
    }

    if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-thumbnailbar"))
    {
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-thumbnailbar")),
                TRUE);
        gtk_widget_show (window->priv->thumbnailbar);
    }
    else
    {
        gtk_check_menu_item_set_active (
                GTK_CHECK_MENU_ITEM (
                        gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/show-thumbnailbar")),
                FALSE);
        gtk_widget_hide (window->priv->thumbnailbar);
    }

    /**
     * Set sort-type
     */
    switch (rstto_settings_get_uint_property (window->priv->settings_manager, "sort-type"))
    {
        case SORT_TYPE_NAME:
            gtk_check_menu_item_set_active (
                    GTK_CHECK_MENU_ITEM (
                            gtk_ui_manager_get_widget (
                                    window->priv->ui_manager,
                                    "/main-menu/edit-menu/sorting-menu/sort-filename")),
                    TRUE);
            break;
        case SORT_TYPE_DATE:
            gtk_check_menu_item_set_active (
                    GTK_CHECK_MENU_ITEM (
                            gtk_ui_manager_get_widget (
                                    window->priv->ui_manager,
                                    "/main-menu/edit-menu/sorting-menu/sort-date")),
                    TRUE);
            break;
        default:
            g_warning("Sort type unsupported");
            break;
    }

    g_signal_connect(G_OBJECT(window), "motion-notify-event", G_CALLBACK(cb_rstto_main_window_motion_notify_event), window);
    g_signal_connect(G_OBJECT(window->priv->image_viewer), "enter-notify-event", G_CALLBACK(cb_rstto_main_window_image_viewer_enter_notify_event), window);
    g_signal_connect(G_OBJECT(window->priv->image_viewer), "scroll-event", G_CALLBACK(cb_rstto_main_window_image_viewer_scroll_event), window);

    g_signal_connect(G_OBJECT(window), "configure-event", G_CALLBACK(cb_rstto_main_window_configure_event), NULL);
    g_signal_connect(G_OBJECT(window), "window-state-event", G_CALLBACK(cb_rstto_main_window_state_event), NULL);
    g_signal_connect(G_OBJECT(window->priv->image_list_toolbar), "button-press-event", G_CALLBACK(cb_rstto_main_window_navigationtoolbar_button_press_event), window);
    g_signal_connect(G_OBJECT(window->priv->thumbnailbar), "button-press-event", G_CALLBACK(cb_rstto_main_window_navigationtoolbar_button_press_event), window);
    g_signal_connect(G_OBJECT(window->priv->image_viewer), "size-ready", G_CALLBACK(cb_rstto_main_window_update_statusbar), window);
    g_signal_connect(G_OBJECT(window->priv->image_viewer), "scale-changed", G_CALLBACK(cb_rstto_main_window_update_statusbar), window);
    g_signal_connect(G_OBJECT(window->priv->image_viewer), "files-dnd", G_CALLBACK(cb_rstto_main_window_dnd_files), window);

    if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
    {
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_fullscreen_merge_id,
                "/file-toolbar/placeholder-fullscreen",
                "fullscreen",
                "fullscreen",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
        gtk_widget_hide (
                gtk_ui_manager_get_widget (
                        window->priv->ui_manager,
                        "/main-menu/view-menu/show-nav-toolbar"));
    }
    else
    {
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_fullscreen_merge_id,
                "/navigation-toolbar/placeholder-fullscreen",
                "fullscreen",
                "fullscreen",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
        gtk_widget_show (
                gtk_ui_manager_get_widget (
                        window->priv->ui_manager,
                        "/main-menu/view-menu/show-nav-toolbar"));
    }


    g_signal_connect (
            G_OBJECT(window->priv->settings_manager),
            "notify::merge-toolbars",
            G_CALLBACK (cb_rstto_merge_toolbars_changed),
            window);
    g_signal_connect (
            G_OBJECT(window->priv->settings_manager),
            "notify::wrap-images",
            G_CALLBACK (cb_rstto_wrap_images_changed),
            window);
    g_signal_connect (
            G_OBJECT(window->priv->settings_manager),
            "notify::desktop-type",
            G_CALLBACK (cb_rstto_desktop_type_changed),
            window);

}

static void
rstto_main_window_class_init(RsttoMainWindowClass *window_class)
{
    GObjectClass *object_class = (GObjectClass*)window_class;
    GtkWidgetClass *widget_class = (GtkWidgetClass *)window_class;

    parent_class = g_type_class_peek_parent(window_class);

    object_class->dispose = rstto_main_window_dispose;

    widget_class->size_allocate = rstto_main_window_size_allocate;
    widget_class->key_press_event = key_press_event;
}

static void
rstto_main_window_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW(widget);
    GtkRequisition   panel_requisition;

    GTK_WIDGET_CLASS (parent_class)->size_allocate(widget, allocation); 

    gtk_widget_size_request (window->priv->vpaned_top, &panel_requisition);

}

static void
rstto_main_window_dispose(GObject *object)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW(object);

    if (window->priv)
    {
        if (window->priv->ui_manager)
        {
            g_object_unref (window->priv->ui_manager);
            window->priv->ui_manager = NULL;
        } 

        if (window->priv->settings_manager)
        {
            g_object_unref (window->priv->settings_manager);
            window->priv->settings_manager = NULL;
        }

        if (window->priv->image_list)
        {
            g_object_unref (window->priv->image_list);
            window->priv->image_list = NULL;
        }

        if (window->priv->filter)
        {
            g_object_unref (window->priv->filter);
            window->priv->filter= NULL;
        }
        g_free (window->priv);
        window->priv = NULL;
    }

    G_OBJECT_CLASS (parent_class)->dispose(object); 
}

/**
 * rstto_main_window_new:
 * @image_list:
 *
 * Return value:
 */
GtkWidget *
rstto_main_window_new (RsttoImageList *image_list, gboolean fullscreen)
{
    RsttoMainWindow *window;

    g_return_val_if_fail (RSTTO_IS_IMAGE_LIST (image_list), NULL);

    window = g_object_new (RSTTO_TYPE_MAIN_WINDOW, NULL);

    window->priv->image_list = image_list;
    g_object_ref (image_list);

    switch (rstto_settings_get_uint_property (window->priv->settings_manager, "sort-type"))
    {
        case SORT_TYPE_NAME:
            rstto_image_list_set_sort_by_name (window->priv->image_list);
            break;
        case SORT_TYPE_DATE:
            rstto_image_list_set_sort_by_date (window->priv->image_list);
            break;
        default:
            g_warning("Sort type unsupported");
            break;
    }


    window->priv->iter = rstto_image_list_get_iter (window->priv->image_list);
    g_signal_connect (
            G_OBJECT (window->priv->iter),
            "changed",
            G_CALLBACK (cb_rstto_main_window_image_list_iter_changed),
            window);
    rstto_thumbnail_bar_set_image_list (
            RSTTO_THUMBNAIL_BAR (window->priv->thumbnailbar),
            window->priv->image_list);
    rstto_thumbnail_bar_set_iter (
            RSTTO_THUMBNAIL_BAR (window->priv->thumbnailbar),
            window->priv->iter);
    rstto_main_window_update_buttons (window);

    if (fullscreen == TRUE)
    {
        gtk_window_fullscreen (GTK_WINDOW (window));
    }

    return GTK_WIDGET (window);
}

/**
 * rstto_main_window_image_list_iter_changed:
 * @window:
 *
 */
static void
rstto_main_window_image_list_iter_changed (RsttoMainWindow *window)
{
    const gchar *file_basename = NULL;
    gchar *title = NULL;
    RsttoFile *cur_file = NULL;
    gint position, count;
    RsttoImageList *image_list = window->priv->image_list;
    GList *app_list, *iter;
    const gchar *content_type;
    GtkWidget *open_with_menu = gtk_menu_new();
    GtkWidget *open_with_window_menu = gtk_menu_new();
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/open-with-menu")), open_with_menu);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/edit-menu/open-with-menu")), open_with_window_menu);

    if (window->priv->image_list)
    {
        position = rstto_image_list_iter_get_position (window->priv->iter);
        count = rstto_image_list_get_n_images (image_list);
        cur_file = rstto_image_list_iter_get_file (window->priv->iter);
        if (NULL != cur_file)
        {
            content_type  = rstto_file_get_content_type (cur_file);

            rstto_image_viewer_set_file (
                    RSTTO_IMAGE_VIEWER(window->priv->image_viewer),
                    cur_file,
                    -1.0,
                    0);

            app_list = g_app_info_get_all_for_type (content_type);

            if (NULL != app_list)
            {
                for (iter = app_list; iter; iter = g_list_next (iter))
                {
                    GtkWidget *menu_item = rstto_app_menu_item_new (iter->data, rstto_file_get_file (cur_file));
                    gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
                    menu_item = rstto_app_menu_item_new (iter->data, rstto_file_get_file (cur_file));
                    gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
                }
            }
            else
            {
            }

            gtk_widget_show_all (open_with_menu);
            gtk_widget_show_all (open_with_window_menu);

            file_basename = rstto_file_get_display_name (cur_file);

            if (count > 1)
            {
                title = g_strdup_printf ("%s - %s [%d/%d]", RISTRETTO_APP_TITLE,  file_basename, position+1, count);
            }
            else
            {
                title = g_strdup_printf ("%s - %s", RISTRETTO_APP_TITLE,  file_basename);
            }

        }
        else
        {
            GtkWidget *menu_item = gtk_image_menu_item_new_with_label (_("Empty"));
            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_menu), menu_item);
            gtk_widget_set_sensitive (menu_item, FALSE);

            rstto_image_viewer_set_file (RSTTO_IMAGE_VIEWER(window->priv->image_viewer), NULL, -1, 0);

            menu_item = gtk_image_menu_item_new_with_label (_("Empty"));
            gtk_menu_shell_append (GTK_MENU_SHELL (open_with_window_menu), menu_item);
            gtk_widget_set_sensitive (menu_item, FALSE);

            gtk_widget_show_all (open_with_menu);
            gtk_widget_show_all (open_with_window_menu);

            title = g_strdup (RISTRETTO_APP_TITLE);
        }

        rstto_main_window_update_buttons (window);
        rstto_main_window_update_statusbar (window);
        gtk_window_set_title (GTK_WINDOW (window), title);
        g_free (title);

    }
}

/**
 * rstto_main_window_update_statusbar:
 * @window:
 *
 */
static void
rstto_main_window_update_statusbar (RsttoMainWindow *window)
{
    const gchar *file_basename = NULL;
    gchar *status = NULL;
    gchar *tmp_status = NULL;
    RsttoFile *cur_file = NULL;
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER(window->priv->image_viewer);
    ExifEntry *exif_entry = NULL;
    gchar exif_data[20];

    if (window->priv->image_list)
    {
        cur_file = rstto_image_list_iter_get_file (window->priv->iter);
        if (NULL != cur_file)
        {
            file_basename = rstto_file_get_display_name (cur_file);

            status = g_strdup(file_basename);

            if (TRUE == rstto_file_has_exif (cur_file))
            {
                /* Extend the status-message with exif-info */
                /********************************************/
                exif_entry = rstto_file_get_exif (
                        cur_file,
                        EXIF_TAG_FNUMBER);
                if (exif_entry)
                {
                    exif_entry_get_value (exif_entry, exif_data, 20);

                    tmp_status = g_strdup_printf ("%s\t%s", status, exif_data);

                    g_free (status);
                    status = tmp_status;

                    /*exif_entry_free (exif_entry);*/
                }
                exif_entry = rstto_file_get_exif (
                        cur_file,
                        EXIF_TAG_EXPOSURE_TIME);
                if (exif_entry)
                {
                    exif_entry_get_value (exif_entry, exif_data, 20);

                    tmp_status = g_strdup_printf ("%s\t%s", status, exif_data);

                    g_free (status);
                    status = tmp_status;

                    /*exif_entry_free (exif_entry);*/
                }
            }

            if(rstto_image_viewer_get_width(viewer) != 0 && rstto_image_viewer_get_height(viewer) != 0)
            {
                tmp_status = g_strdup_printf ("%s\t%d x %d\t%.1f%%", status,
                                            rstto_image_viewer_get_width(viewer),
                                            rstto_image_viewer_get_height(viewer),
                                            (100 * rstto_image_viewer_get_scale(viewer)));

                g_free (status);
                status = tmp_status;
            }
        }
        else
        {
            status = g_strdup (_("Press open to select an image"));
        }

        gtk_statusbar_pop (GTK_STATUSBAR (window->priv->statusbar), window->priv->statusbar_context_id);

        if (status)
        {
            gtk_statusbar_push (GTK_STATUSBAR (window->priv->statusbar), window->priv->statusbar_context_id, status);
            g_free (status);
            status = NULL;
        }

    }

}

/**
 * rstto_main_window_update_buttons:
 * @window:
 * @sensitive:
 *
 */
static void
rstto_main_window_update_buttons (RsttoMainWindow *window)
{
    g_return_if_fail (window->priv->image_list != NULL);
    switch (rstto_image_list_get_n_images (window->priv->image_list))
    {
        case 0: 
            if ( GTK_WIDGET_VISIBLE (window) )
            {
                if ( 0 != (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
                {
                    gtk_widget_show (window->priv->toolbar);
                }
            }
            gtk_widget_hide (window->priv->thumbnailbar);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/save-copy"), FALSE);
            /*
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/print"), FALSE);
            */
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/properties"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/close"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/edit-menu/delete"), FALSE);

            /* Go Menu */
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/forward"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/back"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/first"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/last"), FALSE); 

            gtk_action_set_sensitive (window->priv->play_action, FALSE);
            gtk_action_set_sensitive (window->priv->pause_action, FALSE);

    
            /* Stop the slideshow if no image is opened */
            if (window->priv->playing == TRUE)
            {
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->play_merge_id,
                        "/main-menu/go-menu/placeholder-slideshow",
                        "play",
                        "play",
                        GTK_UI_MANAGER_MENUITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (window->priv->ui_manager, window->priv->pause_merge_id);

                /* Check if the toolbars are merged */
                if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
                {
                    gtk_ui_manager_add_ui (
                            window->priv->ui_manager,
                            window->priv->toolbar_play_merge_id,
                            "/file-toolbar/placeholder-slideshow",
                            "play",
                            "play",
                            GTK_UI_MANAGER_TOOLITEM,
                            FALSE);
                }
                else
                {
                    gtk_ui_manager_add_ui (
                            window->priv->ui_manager,
                            window->priv->toolbar_play_merge_id,
                            "/navigation-toolbar/placeholder-slideshow",
                            "play",
                            "play",
                            GTK_UI_MANAGER_TOOLITEM,
                            FALSE);
                }
                gtk_ui_manager_remove_ui (window->priv->ui_manager, window->priv->toolbar_pause_merge_id);

                window->priv->playing = FALSE;
            }
            

            /* View Menu */
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/set-as-wallpaper"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/zoom-menu"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/rotation-menu"), FALSE);

            /* Toolbar */
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/save-copy"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/close"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/delete"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/forward"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/back"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-in"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-out"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-fit"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-100"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/rotate-ccw"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/rotate-cw"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/forward"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/back"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-in"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-out"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-fit"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-100"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/rotate-ccw"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/rotate-cw"), FALSE);

            /* Image Viewer popup-menu */
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/close"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/open-with-menu"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-in"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-out"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-100"), FALSE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-fit"), FALSE);
            break;
        case 1: 
            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "show-thumbnailbar"))
            {
                if ( 0 == (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
                {
                    gtk_widget_show (window->priv->thumbnailbar);
                }
                else
                {
                    if (rstto_settings_get_boolean_property (
                            window->priv->settings_manager,
                            "hide-thumbnailbar-fullscreen"))
                    {
                        gtk_widget_hide (window->priv->thumbnailbar);
                    }
                    else
                    {
                        gtk_widget_show (window->priv->thumbnailbar);
                    }
                
                }
            }
            if ( 0 != (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
            {
                gtk_widget_hide (window->priv->toolbar);
            }
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/save-copy"), TRUE);
            /*
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/print"), TRUE);
            */
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/properties"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/close"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/edit-menu/delete"), TRUE);

            /* Go Menu */
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/forward"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/back"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/first"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/last"), FALSE); 

            gtk_action_set_sensitive (window->priv->play_action, FALSE);
            gtk_action_set_sensitive (window->priv->pause_action, FALSE);

            /* Stop the slideshow if only one image is opened */
            if (window->priv->playing == TRUE)
            {
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->play_merge_id,
                        "/main-menu/go-menu/placeholder-slideshow",
                        "play",
                        "play",
                        GTK_UI_MANAGER_MENUITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->pause_merge_id);

                /* Check if the toolbars are merged */
                if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
                {
                    gtk_ui_manager_add_ui (
                            window->priv->ui_manager,
                            window->priv->toolbar_play_merge_id,
                            "/file-toolbar/placeholder-slideshow",
                            "play",
                            "play",
                            GTK_UI_MANAGER_TOOLITEM,
                            FALSE);
                }
                else
                {
                    gtk_ui_manager_add_ui (
                            window->priv->ui_manager,
                            window->priv->toolbar_play_merge_id,
                            "/navigation-toolbar/placeholder-slideshow",
                            "play",
                            "play",
                            GTK_UI_MANAGER_TOOLITEM,
                            FALSE);
                }

                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_pause_merge_id);

                window->priv->playing = FALSE;
            }
            

            /* View Menu */
            if (window->priv->wallpaper_manager)
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/set-as-wallpaper"), TRUE);
            }
            else
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/set-as-wallpaper"), FALSE);
            }
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/zoom-menu"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/rotation-menu"), TRUE);

            /* Toolbar */
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/save-copy"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/close"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/delete"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/forward"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/back"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-in"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-out"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-fit"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-100"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/rotate-ccw"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/rotate-cw"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/forward"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/back"), FALSE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-in"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-out"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-fit"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-100"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/rotate-ccw"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/rotate-cw"), TRUE);

            /* Image Viewer popup-menu */
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/close"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/open-with-menu"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-in"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-out"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-100"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-fit"), TRUE);
            break;
        default: 
            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "show-thumbnailbar"))
            {
                if ( 0 == (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
                {
                    gtk_widget_show (window->priv->thumbnailbar);
                }
                else
                {
                    if (rstto_settings_get_boolean_property (
                            window->priv->settings_manager,
                            "hide-thumbnailbar-fullscreen"))
                    {
                        gtk_widget_hide (window->priv->thumbnailbar);
                    }
                    else
                    {
                        gtk_widget_show (window->priv->thumbnailbar);
                    }
                
                }
            }
            if ( 0 != (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
            {
                gtk_widget_hide (window->priv->toolbar);
            }
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/save-copy"), TRUE);
            /*
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/print"), TRUE);
            */
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/properties"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/file-menu/close"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/edit-menu/delete"), TRUE);

            /* Go Menu */
            if (rstto_image_list_iter_has_next (window->priv->iter))
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/forward"), TRUE);
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/forward"), TRUE);
            }
            else
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/forward"), FALSE);
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/forward"), FALSE);
            }
            if (rstto_image_list_iter_has_previous (window->priv->iter))
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/back"), TRUE);
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/back"), TRUE);
            }
            else
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/back"), FALSE);
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/back"), FALSE);
            }
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/first"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget ( window->priv->ui_manager, "/main-menu/go-menu/last"), TRUE); 

            gtk_action_set_sensitive (window->priv->play_action, TRUE);
            gtk_action_set_sensitive (window->priv->pause_action, TRUE);
            

            /* View Menu */
            if (window->priv->wallpaper_manager)
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/set-as-wallpaper"), TRUE);
            }
            else
            {
                gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/set-as-wallpaper"), FALSE);
            }
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/zoom-menu"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/main-menu/view-menu/rotation-menu"), TRUE);

            /* Toolbar */
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/save-copy"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/close"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/delete"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-in"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-out"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-fit"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/zoom-100"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/rotate-ccw"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/file-toolbar/rotate-cw"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/forward"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/back"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-in"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-out"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-fit"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/zoom-100"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/rotate-ccw"), TRUE);
            gtk_widget_set_sensitive (gtk_ui_manager_get_widget (window->priv->ui_manager, "/navigation-toolbar/rotate-cw"), TRUE);

            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/close"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/open-with-menu"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-in"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-out"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-100"), TRUE);
            gtk_widget_set_sensitive ( gtk_ui_manager_get_widget ( window->priv->ui_manager, "/image-viewer-menu/zoom-fit"), TRUE);
            break;
    }

    if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
    {
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/main-menu/view-menu/show-nav-toolbar"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/navigation-toolbar"));

        /* Show buttons */
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/back"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/forward"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/rotate-cw"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/rotate-ccw"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-in"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-out"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-100"));
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-fit"));

        if (window->priv->playing == TRUE)
        {
            gtk_ui_manager_remove_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_pause_merge_id);
            gtk_ui_manager_add_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_pause_merge_id,
                    "/file-toolbar/placeholder-slideshow",
                    "pause",
                    "pause",
                    GTK_UI_MANAGER_TOOLITEM,
                    FALSE);
        }
        else
        {
            gtk_ui_manager_remove_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_play_merge_id);
            gtk_ui_manager_add_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_play_merge_id,
                    "/file-toolbar/placeholder-slideshow",
                    "play",
                    "play",
                    GTK_UI_MANAGER_TOOLITEM,
                    FALSE);
        }
        if ( GTK_WIDGET_VISIBLE (window) )
        {
            gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_unfullscreen_merge_id);
            gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_fullscreen_merge_id);
            /* Do not make the widget visible when in
             * fullscreen mode.
             */
            if ( 0 == (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
            {
                gtk_ui_manager_add_ui (window->priv->ui_manager,
                                       window->priv->toolbar_fullscreen_merge_id,
                                       "/file-toolbar/placeholder-fullscreen",
                                       "fullscreen",
                                       "fullscreen",
                                       GTK_UI_MANAGER_TOOLITEM,
                                       FALSE);
            }
            else
            {
                gtk_ui_manager_add_ui (window->priv->ui_manager,
                                       window->priv->toolbar_unfullscreen_merge_id,
                                       "/file-toolbar/placeholder-fullscreen",
                                       "unfullscreen",
                                       "unfullscreen",
                                       GTK_UI_MANAGER_TOOLITEM,
                                       FALSE);
            }
        }
    }
    else
    {
        gtk_widget_show (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/main-menu/view-menu/show-nav-toolbar"));

        if ( GTK_WIDGET_VISIBLE (window) )
        {
            /* Do not make the widget visible when in
             * fullscreen mode.
             */
            if ( 0 == (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
            {
                if (rstto_settings_get_boolean_property (
                        window->priv->settings_manager,
                        "show-file-toolbar") )
                {
                    gtk_widget_show (
                        gtk_ui_manager_get_widget (
                                window->priv->ui_manager,
                                "/file-toolbar"));
                }
                else
                {
                    gtk_widget_hide (
                        gtk_ui_manager_get_widget (
                                window->priv->ui_manager,
                                "/file-toolbar"));

                }
                if (rstto_settings_get_boolean_property (
                        window->priv->settings_manager,
                        "show-nav-toolbar") )
                {
                    gtk_widget_show (
                        gtk_ui_manager_get_widget (
                                window->priv->ui_manager,
                                "/navigation-toolbar"));
                }
            }
        }


        /* Hide buttons */
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/back"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/forward"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/rotate-cw"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/rotate-ccw"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-in"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-out"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-100"));
        gtk_widget_hide (
            gtk_ui_manager_get_widget (
                    window->priv->ui_manager,
                    "/file-toolbar/zoom-fit"));

        if (window->priv->playing == TRUE)
        {
            gtk_ui_manager_remove_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_pause_merge_id);
            gtk_ui_manager_add_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_pause_merge_id,
                    "/navigation-toolbar/placeholder-slideshow",
                    "pause",
                    "pause",
                    GTK_UI_MANAGER_TOOLITEM,
                    FALSE);
        }
        else
        {
            gtk_ui_manager_remove_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_play_merge_id);
            gtk_ui_manager_add_ui (
                    window->priv->ui_manager,
                    window->priv->toolbar_play_merge_id,
                    "/navigation-toolbar/placeholder-slideshow",
                    "play",
                    "play",
                    GTK_UI_MANAGER_TOOLITEM,
                    FALSE);
        }

        if ( GTK_WIDGET_VISIBLE (window) )
        {
            gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_unfullscreen_merge_id);
            gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_fullscreen_merge_id);
            /* Do not make the widget visible when in
             * fullscreen mode.
             */
            if ( 0 == (gdk_window_get_state (GTK_WIDGET (window)->window) & GDK_WINDOW_STATE_FULLSCREEN ))
            {
                gtk_ui_manager_add_ui (window->priv->ui_manager,
                                       window->priv->toolbar_fullscreen_merge_id,
                                       "/navigation-toolbar/placeholder-fullscreen",
                                       "fullscreen",
                                       "fullscreen",
                                       GTK_UI_MANAGER_TOOLITEM,
                                       FALSE);
            }
            else
            {
                if (rstto_image_list_get_n_images (window->priv->image_list) > 0)
                {
                    gtk_ui_manager_add_ui (window->priv->ui_manager,
                                           window->priv->toolbar_unfullscreen_merge_id,
                                           "/navigation-toolbar/placeholder-fullscreen",
                                           "unfullscreen",
                                           "unfullscreen",
                                           GTK_UI_MANAGER_TOOLITEM,
                                           FALSE);
                }
                else
                {
                    gtk_ui_manager_add_ui (
                            window->priv->ui_manager,
                            window->priv->toolbar_unfullscreen_merge_id,
                            "/file-toolbar/placeholder-fullscreen",
                            "unfullscreen",
                            "unfullscreen",
                            GTK_UI_MANAGER_TOOLITEM,
                            FALSE);

                }
            }
        }

    }
}

static gboolean
rstto_window_save_geometry_timer (gpointer user_data)
{
    GtkWindow *window = GTK_WINDOW(user_data);
    gint width = 0;
    gint height = 0;
    /* check if the window is still visible */
    if (GTK_WIDGET_VISIBLE (window))
    {
        /* determine the current state of the window */
        gint state = gdk_window_get_state (GTK_WIDGET (window)->window);

        /* don't save geometry for maximized or fullscreen windows */
        if ((state & (GDK_WINDOW_STATE_MAXIMIZED | GDK_WINDOW_STATE_FULLSCREEN)) == 0)
        {
            /* determine the current width/height of the window... */
            gtk_window_get_size (GTK_WINDOW (window), &width, &height);

            /* ...and remember them as default for new windows */
            g_object_set (G_OBJECT (RSTTO_MAIN_WINDOW(window)->priv->settings_manager), 
                          "window-width", width,
                          "window-height", height,
                          NULL);
        }
    }
    return FALSE;
}

static void
rstto_main_window_set_navigationbar_position (RsttoMainWindow *window, guint orientation)
{
    rstto_settings_set_navbar_position (window->priv->settings_manager, orientation);

    switch (orientation)
    {
        case 0: /* Left */
            g_object_ref (window->priv->image_list_toolbar);
            g_object_ref (window->priv->thumbnailbar);

            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->back), GTK_STOCK_GO_UP);
            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->forward), GTK_STOCK_GO_DOWN);


            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->thumbnailbar)), window->priv->thumbnailbar);
            gtk_paned_pack1 (GTK_PANED (window->priv->hpaned_left), window->priv->thumbnailbar, FALSE, FALSE);

            gtk_container_remove (GTK_CONTAINER (window->priv->table), window->priv->image_list_toolbar);
            gtk_table_attach (GTK_TABLE (window->priv->table), window->priv->image_list_toolbar, 0, 1, 0, 3, GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
            gtk_orientable_set_orientation (GTK_ORIENTABLE(window->priv->image_list_toolbar), GTK_ORIENTATION_VERTICAL);
            rstto_thumbnail_bar_set_orientation (RSTTO_THUMBNAIL_BAR(window->priv->thumbnailbar), GTK_ORIENTATION_VERTICAL);
            break;
        case 1: /* Right */
            g_object_ref (window->priv->image_list_toolbar);
            g_object_ref (window->priv->thumbnailbar);

            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->back), GTK_STOCK_GO_UP);
            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->forward), GTK_STOCK_GO_DOWN);


            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->thumbnailbar)), window->priv->thumbnailbar);
            gtk_paned_pack2 (GTK_PANED (window->priv->hpaned_right), window->priv->thumbnailbar, FALSE, FALSE);

            gtk_container_remove (GTK_CONTAINER (window->priv->table), window->priv->image_list_toolbar);
            gtk_table_attach (GTK_TABLE (window->priv->table), window->priv->image_list_toolbar, 2, 3, 0, 3, GTK_FILL,GTK_EXPAND|GTK_FILL, 0, 0);
            gtk_orientable_set_orientation (GTK_ORIENTABLE (window->priv->image_list_toolbar), GTK_ORIENTATION_VERTICAL);
            rstto_thumbnail_bar_set_orientation (RSTTO_THUMBNAIL_BAR(window->priv->thumbnailbar), GTK_ORIENTATION_VERTICAL);
            break;
        case 2: /* Top */
            g_object_ref (window->priv->image_list_toolbar);
            g_object_ref (window->priv->thumbnailbar);

            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->back), GTK_STOCK_GO_BACK);
            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->forward), GTK_STOCK_GO_FORWARD);


            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->thumbnailbar)), window->priv->thumbnailbar);
            gtk_paned_pack1 (GTK_PANED (window->priv->vpaned_top), window->priv->thumbnailbar, FALSE, FALSE);

            gtk_container_remove (GTK_CONTAINER (window->priv->table), window->priv->image_list_toolbar);
            gtk_table_attach (GTK_TABLE (window->priv->table), window->priv->image_list_toolbar, 0, 3, 0, 1, GTK_EXPAND|GTK_FILL,GTK_FILL, 0, 0);
            gtk_orientable_set_orientation (GTK_ORIENTABLE (window->priv->image_list_toolbar), GTK_ORIENTATION_HORIZONTAL);
            rstto_thumbnail_bar_set_orientation (RSTTO_THUMBNAIL_BAR(window->priv->thumbnailbar), GTK_ORIENTATION_HORIZONTAL);
            break;
        case 3: /* Bottom */
            g_object_ref (window->priv->image_list_toolbar);
            g_object_ref (window->priv->thumbnailbar);

            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->back), GTK_STOCK_GO_BACK);
            gtk_tool_button_set_stock_id (GTK_TOOL_BUTTON(window->priv->forward), GTK_STOCK_GO_FORWARD);

            gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (window->priv->thumbnailbar)), window->priv->thumbnailbar);
            gtk_paned_pack2 (GTK_PANED (window->priv->vpaned_bottom), window->priv->thumbnailbar, FALSE, FALSE);

            gtk_container_remove (GTK_CONTAINER (window->priv->table), window->priv->image_list_toolbar);
            gtk_table_attach (GTK_TABLE (window->priv->table), window->priv->image_list_toolbar, 0, 3, 2, 3, GTK_EXPAND|GTK_FILL,GTK_FILL, 0, 0);
            gtk_orientable_set_orientation (GTK_ORIENTABLE(window->priv->image_list_toolbar), GTK_ORIENTATION_HORIZONTAL);
            rstto_thumbnail_bar_set_orientation (RSTTO_THUMBNAIL_BAR(window->priv->thumbnailbar), GTK_ORIENTATION_HORIZONTAL);
            break;
        default:
            break;
    }
}


/************************/
/**                    **/
/** Callback functions **/
/**                    **/
/************************/

static void
cb_rstto_main_window_navigationtoolbar_button_press_event (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    int button, event_time;
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (user_data);
    GtkWidget *menu = NULL;
    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
        if (event)
        {
            button = event->button;
            event_time = event->time;
        }
        else
        {
            button = 0;
            event_time = gtk_get_current_event_time ();
        }


        menu = window->priv->position_menu;
        gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 
                  button, event_time);
    }
}

static void
cb_rstto_main_window_image_list_iter_changed (RsttoImageListIter *iter, RsttoMainWindow *window)
{
    rstto_main_window_image_list_iter_changed (window);
}

static void
cb_rstto_main_window_sorting_function_changed (GtkRadioAction *action, GtkRadioAction *current,  RsttoMainWindow *window)
{
    switch (gtk_radio_action_get_current_value (current))
    {
        case 0:  /* Sort by filename */
        default:
            if (window->priv->image_list != NULL)
            {
                rstto_image_list_set_sort_by_name (window->priv->image_list);
                rstto_settings_set_uint_property (window->priv->settings_manager, "sort-type", SORT_TYPE_NAME);
            }
            break;
        case 1: /* Sort by date */
            if (window->priv->image_list != NULL)
            {
                rstto_image_list_set_sort_by_date (window->priv->image_list);
                rstto_settings_set_uint_property (window->priv->settings_manager, "sort-type", SORT_TYPE_DATE);
            }
            break;
    }
}

static void
cb_rstto_main_window_navigationtoolbar_position_changed (GtkRadioAction *action, GtkRadioAction *current,  RsttoMainWindow *window)
{
    rstto_main_window_set_navigationbar_position (window, gtk_radio_action_get_current_value (current));
}

static void
cb_rstto_main_window_set_as_wallpaper (GtkWidget *widget, RsttoMainWindow *window)
{
    gint response = GTK_RESPONSE_APPLY;
    RsttoFile *file = NULL;
    gchar *desktop_type = NULL;
    GtkWidget *dialog = NULL;
    GtkWidget *content_area = NULL;
    GtkWidget *behaviour_desktop_lbl;
    GtkWidget *choose_desktop_combo_box;

    if (window->priv->iter)
    {
        file = rstto_image_list_iter_get_file (window->priv->iter);
    }

    g_return_if_fail (NULL != file);

    desktop_type = rstto_settings_get_string_property (window->priv->settings_manager, "desktop-type");
    if (G_UNLIKELY (NULL == desktop_type))
    {
        /* No desktop has been selected, first time this feature is
         * used. -- Ask the user which method he wants ristretto to
         * apply to set the desktop wallpaper.
         */
        dialog = gtk_dialog_new_with_buttons (
                _("Choose 'set wallpaper' method"),
                GTK_WINDOW(window),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_OK,
                GTK_RESPONSE_OK,
                GTK_STOCK_CANCEL,
                GTK_RESPONSE_CANCEL,
                NULL);

        /* Populate the dialog */
        content_area = gtk_dialog_get_content_area (GTK_DIALOG(dialog));

        behaviour_desktop_lbl = gtk_label_new(NULL);
        gtk_label_set_markup (
                GTK_LABEL (behaviour_desktop_lbl),
                _("Configure which system is currently managing your desktop.\n"
                  "This setting determines the method <i>Ristretto</i> will use\n"
                  "to configure the desktop wallpaper."));
        gtk_misc_set_alignment(
                GTK_MISC(behaviour_desktop_lbl),
                0,
                0.5);
        gtk_box_pack_start (
                GTK_BOX (content_area),
                behaviour_desktop_lbl,
                FALSE,
                FALSE,
                0);

        choose_desktop_combo_box =
                gtk_combo_box_text_new();
        gtk_box_pack_start (
                GTK_BOX (content_area),
                choose_desktop_combo_box,
                FALSE,
                FALSE,
                0);
        gtk_combo_box_text_insert_text(
                GTK_COMBO_BOX_TEXT (choose_desktop_combo_box),
                DESKTOP_TYPE_NONE,
                _("None"));
        gtk_combo_box_text_insert_text (
                GTK_COMBO_BOX_TEXT (choose_desktop_combo_box),
                DESKTOP_TYPE_XFCE,
                _("Xfce"));
        gtk_combo_box_text_insert_text (
                GTK_COMBO_BOX_TEXT (choose_desktop_combo_box),
                DESKTOP_TYPE_GNOME,
                _("GNOME"));

        gtk_combo_box_set_active (
            GTK_COMBO_BOX (choose_desktop_combo_box),
            DESKTOP_TYPE_XFCE);

        gtk_widget_show_all (content_area);
        


        /* Show the dialog */
        response = gtk_dialog_run (GTK_DIALOG(dialog));

        /* If the response was 'OK', the user has made a choice */
        if ( GTK_RESPONSE_OK == response )
        {
            switch (gtk_combo_box_get_active (
                    GTK_COMBO_BOX (choose_desktop_combo_box)))
            {
                case DESKTOP_TYPE_NONE:
                    desktop_type = g_strdup ("none");
                    if (NULL != window->priv->wallpaper_manager)
                    {
                        g_object_unref (window->priv->wallpaper_manager);
                        window->priv->wallpaper_manager = NULL;
                    }
                    break;
                case DESKTOP_TYPE_XFCE:
                    desktop_type = g_strdup ("xfce");
                    if (NULL != window->priv->wallpaper_manager)
                    {
                        g_object_unref (window->priv->wallpaper_manager);
                    }
                    window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new ();
                    break;
                case DESKTOP_TYPE_GNOME:
                    desktop_type = g_strdup ("gnome");
                    if (NULL != window->priv->wallpaper_manager)
                    {
                        g_object_unref (window->priv->wallpaper_manager);
                    }
                    window->priv->wallpaper_manager = rstto_gnome_wallpaper_manager_new ();
                    break;
            }
	    rstto_settings_set_string_property (
		    window->priv->settings_manager,
		    "desktop-type",
		    desktop_type);
        
        }

        /* Clean-up the dialog */
        gtk_widget_destroy (dialog);
        dialog = NULL;
    }

    if (NULL != desktop_type && NULL != window->priv->wallpaper_manager)
    {
        /* Set the response to GTK_RESPONSE_APPLY,
         * so we at least do one run.
         */
        response = GTK_RESPONSE_APPLY;
        while (GTK_RESPONSE_APPLY == response)
        {
            response = rstto_wallpaper_manager_configure_dialog_run (window->priv->wallpaper_manager, file);
            switch (response)
            {
                case GTK_RESPONSE_OK:
                case GTK_RESPONSE_APPLY:
                    rstto_wallpaper_manager_set (window->priv->wallpaper_manager, file);
                    break;
            }
        }
    }

    if (G_LIKELY (NULL != desktop_type))
    {
        g_free (desktop_type);
        desktop_type = NULL;
    }
}

static void
cb_rstto_main_window_state_event(GtkWidget *widget, GdkEventWindowState *event, gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW(widget);

    if(event->changed_mask & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if(event->new_window_state & GDK_WINDOW_STATE_FULLSCREEN)
        {
            gtk_widget_hide (window->priv->menubar);
            if (rstto_image_list_get_n_images (window->priv->image_list) != 0)
            {
                gtk_widget_hide (window->priv->toolbar);
            }
            else
            {
                gtk_widget_show (window->priv->toolbar);
            }
            gtk_widget_hide (window->priv->statusbar);
            if (window->priv->fs_toolbar_sticky)
            {
                if (window->priv->show_fs_toolbar_timeout_id > 0)
                {
                    g_source_remove (window->priv->show_fs_toolbar_timeout_id);
                    window->priv->show_fs_toolbar_timeout_id = 0;
                }
                if (rstto_image_list_get_n_images (window->priv->image_list) != 0)
                {
                    window->priv->show_fs_toolbar_timeout_id = g_timeout_add (500, (GSourceFunc)cb_rstto_main_window_show_fs_toolbar_timeout, window);
                }
            }
            else
            {
                gtk_widget_hide (window->priv->image_list_toolbar);
            }

            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "hide-thumbnailbar-fullscreen"))
            {
                gtk_widget_hide (window->priv->thumbnailbar);
            }

            if (rstto_settings_get_boolean_property (
                    window->priv->settings_manager,
                    "merge-toolbars") ||
                rstto_image_list_get_n_images (window->priv->image_list) == 0)
            {
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_unfullscreen_merge_id,
                        "/file-toolbar/placeholder-fullscreen",
                        "unfullscreen",
                        "unfullscreen",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_fullscreen_merge_id);
            }
            else
            {
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_unfullscreen_merge_id,
                        "/navigation-toolbar/placeholder-fullscreen",
                        "unfullscreen",
                        "unfullscreen",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_fullscreen_merge_id);
            }
        }
        else
        {
            if (rstto_settings_get_boolean_property (
                    window->priv->settings_manager,
                    "merge-toolbars"))
            {
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_fullscreen_merge_id,
                        "/file-toolbar/placeholder-fullscreen",
                        "fullscreen",
                        "fullscreen",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_unfullscreen_merge_id);

                if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-file-toolbar"))
                    gtk_widget_show (window->priv->toolbar);
                else
                    gtk_widget_hide(window->priv->toolbar);
            }
            else
            {
                gtk_ui_manager_add_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_fullscreen_merge_id,
                        "/navigation-toolbar/placeholder-fullscreen",
                        "fullscreen",
                        "fullscreen",
                        GTK_UI_MANAGER_TOOLITEM,
                        FALSE);
                gtk_ui_manager_remove_ui (
                        window->priv->ui_manager,
                        window->priv->toolbar_unfullscreen_merge_id);

                if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-nav-toolbar"))
                    gtk_widget_show (window->priv->image_list_toolbar);
                else
                    gtk_widget_hide(window->priv->image_list_toolbar);

                if (rstto_settings_get_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-file-toolbar"))
                    gtk_widget_show (window->priv->toolbar);
                else
                    gtk_widget_hide(window->priv->toolbar);
            }
            if (window->priv->show_fs_toolbar_timeout_id > 0)
            {
                g_source_remove (window->priv->show_fs_toolbar_timeout_id);
                window->priv->show_fs_toolbar_timeout_id = 0;
            }

            gtk_widget_show (window->priv->menubar);
            gtk_widget_show (window->priv->statusbar);

            if (rstto_settings_get_boolean_property (window->priv->settings_manager, "show-thumbnailbar"))
            {
                if (rstto_image_list_get_n_images (window->priv->image_list) > 0)
                {
                    gtk_widget_show (window->priv->thumbnailbar);
                }
            }
            
        }
    }
    if (event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED)
    {
    }
}

static gboolean 
cb_rstto_main_window_motion_notify_event (RsttoMainWindow *window,
                                         GdkEventMotion *event,
                                         gpointer user_data)
{
    gint width, height;
    if(gdk_window_get_state(GTK_WIDGET(window)->window) & GDK_WINDOW_STATE_FULLSCREEN)
    {
        gdk_drawable_get_size (GDK_DRAWABLE(GTK_WIDGET(window)->window), &width, &height);

        if ((event->x_root == 0) || (event->y_root == 0) || (((gint)event->x_root) == (width-1)) || (((gint)event->y_root) == (height-1)))
        {
            if (rstto_image_list_get_n_images (window->priv->image_list) != 0)
            {
                if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
                {
                    gtk_widget_show (window->priv->toolbar);
                }
                else
                {
                    gtk_widget_show (window->priv->image_list_toolbar);
                }
                window->priv->fs_toolbar_sticky = TRUE;

                if (window->priv->show_fs_toolbar_timeout_id > 0)
                {
                    g_source_remove (window->priv->show_fs_toolbar_timeout_id);
                    window->priv->show_fs_toolbar_timeout_id = 0;
                }
            }
        }
    }
    return TRUE;
}

static gboolean
cb_rstto_main_window_image_viewer_scroll_event (GtkWidget *widget,
                                                GdkEventScroll *event,
                                                gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (user_data);
    if (!(event->state & (GDK_CONTROL_MASK)))
    {
        switch(event->direction)
        {
            case GDK_SCROLL_UP:
            case GDK_SCROLL_LEFT:
                rstto_image_list_iter_previous (window->priv->iter);
                break;
            case GDK_SCROLL_DOWN:
            case GDK_SCROLL_RIGHT:
                rstto_image_list_iter_next (window->priv->iter);
                break;
        }
    }
    return FALSE;
}

static gboolean
cb_rstto_main_window_image_viewer_enter_notify_event (GtkWidget *widget,
                                                      GdkEventCrossing *event,
                                                      gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (user_data);
    if(gdk_window_get_state(GTK_WIDGET(window)->window) & GDK_WINDOW_STATE_FULLSCREEN)
    {
        if (rstto_image_list_get_n_images (window->priv->image_list) != 0)
        {
            window->priv->fs_toolbar_sticky = FALSE;
            if (window->priv->show_fs_toolbar_timeout_id > 0)
            {
                g_source_remove (window->priv->show_fs_toolbar_timeout_id);
                window->priv->show_fs_toolbar_timeout_id = 0;
            }
            window->priv->show_fs_toolbar_timeout_id = g_timeout_add (500, (GSourceFunc)cb_rstto_main_window_show_fs_toolbar_timeout, window);
        }
    }

    return TRUE;
}

static gboolean
cb_rstto_main_window_show_fs_toolbar_timeout (RsttoMainWindow *window)
{
    gtk_widget_hide (window->priv->toolbar);
    gtk_widget_hide (window->priv->image_list_toolbar);
    return FALSE;
}

/**
 * cb_rstto_main_window_play:
 * @widget:
 * @window:
 *
 * Remove the play button from the menu, and add the pause button.
 *
 */
static void
cb_rstto_main_window_play (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_play_slideshow (window);
}

/**
 * cb_rstto_main_window_pause:
 * @widget:
 * @window:
 *
 * Remove the pause button from the menu, and add the play button.
 *
 */
static void
cb_rstto_main_window_pause (GtkWidget *widget, RsttoMainWindow *window)
{
    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->play_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "play",
                           "play",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_remove_ui (window->priv->ui_manager,
                              window->priv->pause_merge_id);

    if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
    {
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_play_merge_id,
                "/file-toolbar/placeholder-slideshow",
                "play",
                "play",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
        gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_pause_merge_id);
    }
    else
    {
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_play_merge_id,
                "/navigation-toolbar/placeholder-slideshow",
                "play",
                "play",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
        gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_pause_merge_id);
    }

    window->priv->playing = FALSE;
}

/**
 * cb_rstto_main_window_play_slideshow:
 * @window:
 *
 */
static gboolean
cb_rstto_main_window_play_slideshow (RsttoMainWindow *window)
{
    if (window->priv->playing)
    {
        /* Check if we could navigate forward, if not, wrapping is
         * disabled and we should force the iter to position 0
         */
        if (rstto_image_list_iter_next (window->priv->iter) == FALSE)
        {
            rstto_image_list_iter_set_position (window->priv->iter, 0);
        }
    }
    else
    {
        window->priv->play_timeout_id  = 0;
    }
    return window->priv->playing;
}

/**
 * cb_rstto_main_window_fullscreen:
 * @widget:
 * @window:
 *
 * Toggle the fullscreen mode of this window.
 *
 */
static void
cb_rstto_main_window_fullscreen (GtkWidget *widget, RsttoMainWindow *window)
{
    if(gdk_window_get_state(GTK_WIDGET(window)->window) & GDK_WINDOW_STATE_FULLSCREEN)
    {
        gtk_window_unfullscreen(GTK_WINDOW(window));
    }
    else
    {
        gtk_window_fullscreen(GTK_WINDOW(window));
    }
}

/**
 * cb_rstto_main_window_preferences:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_preferences (GtkWidget *widget, RsttoMainWindow *window)
{
    GtkWidget *dialog = rstto_preferences_dialog_new (GTK_WINDOW (window));

    gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);
}

/**
 * cb_rstto_main_window_about:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_about (GtkWidget *widget, RsttoMainWindow *window)
{
    const gchar *authors[] = {
      _("Developer:"),
        "Stephan Arts <stephan@xfce.org>",
        NULL};

    GtkWidget *about_dialog = gtk_about_dialog_new();

    gtk_about_dialog_set_version((GtkAboutDialog *)about_dialog, PACKAGE_VERSION);

    gtk_about_dialog_set_comments((GtkAboutDialog *)about_dialog,
        _("Ristretto is an image viewer for the Xfce desktop environment."));
    gtk_about_dialog_set_website((GtkAboutDialog *)about_dialog,
        "http://goodies.xfce.org/projects/applications/ristretto");
    gtk_about_dialog_set_logo_icon_name((GtkAboutDialog *)about_dialog,
        "ristretto");
    gtk_about_dialog_set_authors((GtkAboutDialog *)about_dialog,
        authors);
    gtk_about_dialog_set_translator_credits((GtkAboutDialog *)about_dialog,
        _("translator-credits"));
    gtk_about_dialog_set_license((GtkAboutDialog *)about_dialog,
        xfce_get_license_text(XFCE_LICENSE_TEXT_GPL));
    gtk_about_dialog_set_copyright((GtkAboutDialog *)about_dialog,
        "Copyright \302\251 2006-2011 Stephan Arts");

    gtk_dialog_run(GTK_DIALOG(about_dialog));

    gtk_widget_destroy(about_dialog);
}

/**
 * cb_rstto_main_window_contents:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_contents (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_launch_help ();
}


/**
 * cb_rstto_main_window_quit:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_quit (GtkWidget *widget, RsttoMainWindow *window)
{
    gtk_widget_destroy (GTK_WIDGET (window));
}

static gboolean
cb_rstto_main_window_configure_event (GtkWidget *widget, GdkEventConfigure *event)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW(widget);
    /* shamelessly copied from thunar, written by benny */
    /* check if we have a new dimension here */
    if (widget->allocation.width != event->width || widget->allocation.height != event->height)
    {
        /* drop any previous timer source */
        if (window->priv->window_save_geometry_timer_id > 0)
        {
            g_source_remove (window->priv->window_save_geometry_timer_id);
        }
        window->priv->window_save_geometry_timer_id = 0;

        /* check if we should schedule another save timer */
        if (GTK_WIDGET_VISIBLE (widget))
        {
            /* save the geometry one second after the last configure event */
            window->priv->window_save_geometry_timer_id = g_timeout_add (
                    1000, rstto_window_save_geometry_timer,
                    widget);
        }
    }

    /* let Gtk+ handle the configure event */
    return FALSE;
}

static void
cb_rstto_main_window_update_statusbar (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_main_window_update_statusbar(window);
}

/******************/
/* ZOOM CALLBACKS */
/******************/

/**
 * cb_rstto_main_window_zoom_fit:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_fit (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER(window->priv->image_viewer), 0);
}

/**
 * cb_rstto_main_window_zoom_100:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_100 (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER(window->priv->image_viewer), 1);
}

/**
 * cb_rstto_main_window_zoom_in:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_in (GtkWidget *widget, RsttoMainWindow *window)
{
    gdouble scale = rstto_image_viewer_get_scale (RSTTO_IMAGE_VIEWER(window->priv->image_viewer));
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER(window->priv->image_viewer), scale*1.2);
}

/**
 * cb_rstto_main_window_zoom_out:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_zoom_out (GtkWidget *widget, RsttoMainWindow *window)
{
    gdouble scale = rstto_image_viewer_get_scale (RSTTO_IMAGE_VIEWER(window->priv->image_viewer));
    rstto_image_viewer_set_scale (RSTTO_IMAGE_VIEWER(window->priv->image_viewer), scale/1.2);
}

/**********************/
/* ROTATION CALLBACKS */
/**********************/

/**
 * cb_rstto_main_window_rotate_cw:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_rotate_cw (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER(window->priv->image_viewer);
    switch (rstto_image_viewer_get_orientation (viewer))
    {
        default:
        case RSTTO_IMAGE_ORIENT_NONE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_90);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_180);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_270);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_NONE);
            break;
    }
    rstto_main_window_update_statusbar(window);
}

/**
 * cb_rstto_main_window_rotate_ccw:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_rotate_ccw (GtkWidget *widget, RsttoMainWindow *window)
{
    RsttoImageViewer *viewer = RSTTO_IMAGE_VIEWER(window->priv->image_viewer);
    switch (rstto_image_viewer_get_orientation (viewer))
    {
        default:
        case RSTTO_IMAGE_ORIENT_NONE:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_270);
            break;
        case RSTTO_IMAGE_ORIENT_90:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_NONE);
            break;
        case RSTTO_IMAGE_ORIENT_180:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_90);
            break;
        case RSTTO_IMAGE_ORIENT_270:
            rstto_image_viewer_set_orientation (viewer, RSTTO_IMAGE_ORIENT_180);
            break;
    }
    rstto_main_window_update_statusbar(window);
}


/************************/
/* NAVIGATION CALLBACKS */
/************************/

/**
 * cb_rstto_main_window_first_image:
 * @widget:
 * @window:
 *
 * Move the iter to the first image;
 *
 */
static void
cb_rstto_main_window_first_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_list_iter_set_position (window->priv->iter, 0);
}


/**
 * cb_rstto_main_window_last_image:
 * @widget:
 * @window:
 *
 * Move the iter to the last image;
 *
 */
static void
cb_rstto_main_window_last_image (GtkWidget *widget, RsttoMainWindow *window)
{
    guint n_images = rstto_image_list_get_n_images (window->priv->image_list);
    rstto_image_list_iter_set_position (window->priv->iter, n_images-1);
}

/**
 * cb_rstto_main_window_next_image:
 * @widget:
 * @window:
 *
 * Move the iter to the next image;
 *
 */
static void
cb_rstto_main_window_next_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_list_iter_next (window->priv->iter);
}

/**
 * cb_rstto_main_window_previous_image:
 * @widget:
 * @window:
 *
 * Move the iter to the previous image;
 *
 */
static void
cb_rstto_main_window_previous_image (GtkWidget *widget, RsttoMainWindow *window)
{
    rstto_image_list_iter_previous (window->priv->iter);
}

/**********************/
/* FILE I/O CALLBACKS */
/**********************/

/**
 * cb_rstto_main_window_open_image:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_open_image (GtkWidget *widget, RsttoMainWindow *window)
{
    GtkWidget *dialog, *err_dialog;
    gint response;
    GFile *file;
    GFile *p_file;
    GSList *files = NULL, *_files_iter;
    GValue current_uri_val = {0, };
    GtkFileFilter *filter;
    RsttoFile *r_file = NULL;

    g_value_init (&current_uri_val, G_TYPE_STRING);
    g_object_get_property (G_OBJECT(window->priv->settings_manager), "current-uri", &current_uri_val);

    filter = gtk_file_filter_new();

    dialog = gtk_file_chooser_dialog_new(_("Open image"),
                                         GTK_WINDOW(window),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_OK,
                                         NULL);

    gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dialog), FALSE);

    if (g_value_get_string (&current_uri_val))
    {
        if (strlen (g_value_get_string (&current_uri_val)) > 0)
        {
            gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dialog), g_value_get_string (&current_uri_val));
        }
    }

    gtk_file_filter_add_pixbuf_formats (filter);
    gtk_file_filter_set_name (filter, _("Images"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_add_mime_type (filter, "image/jpeg");
    gtk_file_filter_set_name (filter, _(".jp(e)g"));
    gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);


    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_hide (dialog);
    if(response == GTK_RESPONSE_OK)
    {
        files = gtk_file_chooser_get_files (GTK_FILE_CHOOSER (dialog));
        _files_iter = files;
        if (g_slist_length (files) > 1)
        {
            while (_files_iter)
            {
                file = _files_iter->data;
                if (g_file_query_exists (file, NULL) )
                {
                    r_file = rstto_file_new (file);
                    if (NULL != r_file)
                    {
                        if (rstto_image_list_add_file (window->priv->image_list, r_file, NULL) == FALSE)
                        {
                            err_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                            GTK_DIALOG_MODAL,
                                                            GTK_MESSAGE_ERROR,
                                                            GTK_BUTTONS_OK,
                                                            _("Could not open file"));
                            gtk_dialog_run(GTK_DIALOG(err_dialog));
                            gtk_widget_destroy(err_dialog);
                        }
                        else
                        {
                            /* Add a reference to the file, it is owned by the
                             * sourcefunc and will be unref-ed by it.
                             */
                            g_object_ref (file);
                            g_idle_add_full(G_PRIORITY_LOW, (GSourceFunc) rstto_main_window_add_file_to_recent_files, file, NULL);
                        }
                        g_object_unref (G_OBJECT (r_file));
                        r_file = NULL;
                    }
                }

                _files_iter = g_slist_next (_files_iter);
            }
            rstto_image_list_iter_set_position (
                window->priv->iter,
                0);
        }
        else
        {
            if (g_slist_length (files) == 1)
            {
                if (g_file_query_exists (files->data, NULL) )
                {
                    r_file = rstto_file_new (files->data);

                    p_file = g_file_get_parent (files->data);
                    rstto_image_list_set_directory (
                            window->priv->image_list,
                            p_file,
                            NULL );
                    rstto_image_list_iter_find_file (
                            window->priv->iter,
                            r_file );

                    g_object_unref (r_file);
                }
            }
        }
 
        g_value_set_string (&current_uri_val, gtk_file_chooser_get_current_folder_uri (GTK_FILE_CHOOSER (dialog)));
        g_object_set_property (G_OBJECT(window->priv->settings_manager), "current-uri", &current_uri_val);

    }

    gtk_widget_destroy(dialog);

    rstto_main_window_update_buttons (window);

    if (files)
    {
        g_slist_foreach (files, (GFunc)g_object_unref, NULL);
        g_slist_free (files);
    }
}

/**
 * cb_rstto_main_window_open_recent:
 * @chooser:
 * @window:
 *
 */
static void
cb_rstto_main_window_open_recent(GtkRecentChooser *chooser, RsttoMainWindow *window)
{
    GtkWidget *err_dialog;
    gchar *uri = gtk_recent_chooser_get_current_uri (chooser);
    GError *error = NULL;
    GFile *file = g_file_new_for_uri (uri);
    GFile *p_file;
    RsttoFile *r_file = NULL;

    if ((error == NULL) &&
        (g_file_query_exists (file, NULL)))
    {
        r_file = rstto_file_new (file);
        if ( NULL != r_file )
        {
            p_file = g_file_get_parent (file);
            rstto_image_list_set_directory (
                    window->priv->image_list,
                    p_file,
                    NULL);
            rstto_image_list_iter_find_file (
                    window->priv->iter,
                    r_file );

            g_object_unref (G_OBJECT (r_file));
            r_file = NULL;
        }
    }
    else
    {
        err_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                        GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_OK,
                                        _("Could not open file"));
        gtk_dialog_run (GTK_DIALOG (err_dialog));
        gtk_widget_destroy (err_dialog);

        /* Something is wrong with the file (perhaps it was removed?),
         * remove the item from the recently-used list.
         */
        gtk_recent_manager_remove_item (
                window->priv->recent_manager,
                uri,
                NULL);
    }

    rstto_main_window_update_buttons (window);

    g_object_unref (file);
    g_free (uri);
}

/**
 * cb_rstto_main_window_save_copy:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_save_copy (GtkWidget *widget, RsttoMainWindow *window)
{
    GtkWidget *dialog, *err_dialog;
    gint response;
    GFile *file, *s_file;

    dialog = gtk_file_chooser_dialog_new(_("Save copy"),
                                         GTK_WINDOW(window),
                                         GTK_FILE_CHOOSER_ACTION_SAVE,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_SAVE, GTK_RESPONSE_OK,
                                         NULL);
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response == GTK_RESPONSE_OK)
    {
        file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
        s_file = rstto_file_get_file(rstto_image_list_iter_get_file (window->priv->iter));
        if ( FALSE == g_file_copy (
                s_file,
                file,
                G_FILE_COPY_OVERWRITE,
                NULL,
                NULL,
                NULL,
                NULL) )
        {
            err_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                            GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_OK,
                                            _("Could not save file"));
            gtk_dialog_run(GTK_DIALOG(err_dialog));
            gtk_widget_destroy(err_dialog);
        }
    }

    gtk_widget_destroy(dialog);

}

static void
cb_rstto_main_window_properties (GtkWidget *widget, RsttoMainWindow *window)
{
    /* The display object is owned by gdk, do not unref it */
    GdkDisplay *display = gdk_display_get_default();
    GError *error = NULL;
    RsttoFile *file = rstto_image_list_iter_get_file (window->priv->iter);
    const gchar *uri = NULL;
    GtkWidget *dialog = NULL;
    gboolean use_thunar_properties = rstto_settings_get_boolean_property (
            window->priv->settings_manager,
            "use-thunar-properties");

    if (NULL != file)
    {
        /* Check if we should first ask Thunar
         * to show the file properties dialog.
         */
        if ( TRUE == use_thunar_properties )
        {
            /* Get the file-uri */
            uri = rstto_file_get_uri(file);

            /* Call the DisplayFileProperties dbus
             * interface. If it fails, fall back to the
             * internal properties-dialog.
             */
            if(dbus_g_proxy_call(window->priv->filemanager_proxy,
                                 "DisplayFileProperties",
                                 &error,
                                 G_TYPE_STRING, uri,
                                 G_TYPE_STRING, gdk_display_get_name(display),
                                 G_TYPE_STRING, "",
                                 G_TYPE_INVALID,
                                 G_TYPE_INVALID) == FALSE)
            {
                g_warning("DBUS CALL FAILED: '%s'", error->message);

                /* Create the internal file-properties dialog */
                dialog = rstto_properties_dialog_new (
                        GTK_WINDOW (window),
                        file);

                gtk_dialog_run (GTK_DIALOG(dialog));

                /* Cleanup the file-properties dialog */
                gtk_widget_destroy(dialog);
            }
        }
        else
        {
            /* Create the internal file-properties dialog */
            dialog = rstto_properties_dialog_new (
                    GTK_WINDOW (window),
                    file);

            gtk_dialog_run (GTK_DIALOG(dialog));

            /* Cleanup the file-properties dialog */
            gtk_widget_destroy(dialog);
        }
    }
}

/**
 * cb_rstto_main_window_close:
 * @widget:
 * @window:
 *
 * Close all images.
 *
 * Set the directory to NULL, the image-list-iter will emit an 
 * 'iter-changed' signal. The ui will be updated in response to 
 * that just like it is when an image is opened.
 */
static void
cb_rstto_main_window_close (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    rstto_image_list_set_directory (
            window->priv->image_list,
            NULL,
            NULL);
}

/**
 * cb_rstto_main_window_delete:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_delete (
        GtkWidget *widget,
        RsttoMainWindow *window )
{
    RsttoFile *file = rstto_image_list_iter_get_file (window->priv->iter);
    const gchar *file_basename = rstto_file_get_display_name(file);
    GtkWidget *dialog;
    g_return_if_fail (rstto_image_list_get_n_images (window->priv->image_list) > 0);

    dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_WARNING,
                                                GTK_BUTTONS_OK_CANCEL,
                                                _("Are you sure you want to delete image '%s' from disk?"),
                                                file_basename);

    g_object_ref (file);
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        if (g_file_trash (rstto_file_get_file(file), NULL, NULL) == TRUE)
        {
            rstto_image_list_remove_file (window->priv->image_list, file);
        }
        else
        {
            
        }
    }
    gtk_widget_destroy (dialog);
    g_object_unref (file);
}

static gboolean
rstto_main_window_is_valid_image (RsttoMainWindow *window,
                                  RsttoFile *file)
{
    GtkFileFilterInfo filter_info;

    filter_info.contains =  GTK_FILE_FILTER_MIME_TYPE | GTK_FILE_FILTER_URI;
    filter_info.uri = rstto_file_get_uri (file);
    filter_info.mime_type = rstto_file_get_content_type (file);

    return gtk_file_filter_filter (window->priv->filter, &filter_info);
}

/**
 * cb_rstto_main_window_dnd_files:
 * @widget:
 * @uris:
 * @window:
 *
 */
static void
cb_rstto_main_window_dnd_files (GtkWidget *widget,
                                gchar **uris,
                                RsttoMainWindow *window)
{
    RsttoFile *file;
    guint n;
    gboolean first = TRUE;

    g_return_if_fail ( RSTTO_IS_MAIN_WINDOW(window) );

    for (n = 0; n < g_strv_length (uris); n++)
    {
        file = rstto_file_new (g_file_new_for_uri (uris[n]));

        if ( TRUE == rstto_main_window_is_valid_image (window, file))
        {
            if ( TRUE == first )
            {
                first = FALSE;

                /* On the first valid image, we reset the thumbnailbar. */
                rstto_image_list_set_directory (
                                            window->priv->image_list,
                                            NULL,
                                            NULL);

                /* User dropped a single image, load all images in the
                 * directory and select the image.
                 */
                if (n + 1 == g_strv_length (uris))
                {
                    GFile *p_file;
                    p_file = g_file_get_parent (rstto_file_get_file (file));
                    rstto_image_list_set_directory (window->priv->image_list,
                                                    p_file,
                                                    NULL );
                    rstto_image_list_iter_find_file (window->priv->iter,
                                                     file );

                    return;
                }
            }

            /* User dropped a selection of images, load only them. */
            rstto_image_list_add_file ( window->priv->image_list, file, NULL);
            rstto_image_list_iter_find_file ( window->priv->iter,
                                              file );
        }
        else if ( g_file_query_file_type ( rstto_file_get_file (file),
                                           G_FILE_QUERY_INFO_NONE,
                                           NULL) == G_FILE_TYPE_DIRECTORY)
        {
            GFileEnumerator *enumerator;

            /* User dropped in a directory, get the files in it */
            enumerator = g_file_enumerate_children (
                            rstto_file_get_file (file),
                            "standard::name,access::can-read",
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            NULL);

            if (enumerator)
            {
                GFileInfo *f_info;
                RsttoFile *child;

                /* Check all the files for a valid image */
                for (f_info = g_file_enumerator_next_file (enumerator, NULL, NULL);
                     f_info != NULL;
                     f_info = g_file_enumerator_next_file (enumerator, NULL, NULL))
                {
                    gchar *path = g_strdup_printf ("%s/%s",
                                                   rstto_file_get_path (file),
                                                   g_file_info_get_name (f_info));

                    child = rstto_file_new (g_file_new_for_path (path));

                    g_object_unref (f_info);
                    g_free (path);

                    if ( TRUE == rstto_main_window_is_valid_image (
                                                                window,
                                                                child))
                    {
                        /* Found a valid image, use the directory
                         * and select the first image in the dir */
                        rstto_image_list_set_directory (
                                            window->priv->image_list,
                                            rstto_file_get_file (file),
                                            NULL );
                        rstto_image_list_iter_find_file (
                                                    window->priv->iter,
                                                    child );

                        break;
                    }
                    /* Not a valid image file */
                    g_object_unref (child);
                }
                g_file_enumerator_close (enumerator, NULL, NULL);
                g_object_unref (enumerator);
            }
        }
        else
        {
            /* Not an image file or directory */
            g_object_unref (file);
        }
    }
}

/**********************/
/* PRINTING CALLBACKS */
/**********************/

/*************************/
/* GUI-RELATED CALLBACKS */
/*************************/

/**
 * cb_rstto_main_window_toggle_show_file_toolbar:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_toggle_show_file_toolbar (GtkWidget *widget, RsttoMainWindow *window)
{
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (widget)))
    {
        gtk_widget_show (window->priv->toolbar);
        rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-file-toolbar", TRUE);
    }
    else
    {
        gtk_widget_hide (window->priv->toolbar);
        rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-file-toolbar", FALSE);
    }
}

/**
 * cb_rstto_main_window_toggle_show_nav_toolbar:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_toggle_show_nav_toolbar (GtkWidget *widget, RsttoMainWindow *window)
{
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (widget)))
    {
        gtk_widget_show (window->priv->image_list_toolbar);
        rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-nav-toolbar", TRUE);
    }
    else
    {
        gtk_widget_hide (window->priv->image_list_toolbar);
        rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-nav-toolbar", FALSE);
    }
}

/**
 * cb_rstto_main_window_toggle_show_thumbnailbar:
 * @widget:
 * @window:
 *
 *
 */
static void
cb_rstto_main_window_toggle_show_thumbnailbar (GtkWidget *widget, RsttoMainWindow *window)
{
    if (gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (widget)))
    {
        gtk_widget_show (window->priv->thumbnailbar);
        rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-thumbnailbar", TRUE);
    }
    else
    {
        gtk_widget_hide (window->priv->thumbnailbar);
        rstto_settings_set_boolean_property (RSTTO_SETTINGS (window->priv->settings_manager), "show-thumbnailbar", FALSE);
    }
}

RsttoImageListIter *
rstto_main_window_get_iter (
        RsttoMainWindow *window)
{
    return window->priv->iter;
}

gboolean
rstto_main_window_add_file_to_recent_files (GFile *file)
{
    GFileInfo *file_info;
    GtkRecentData *recent_data;
    gchar* uri;
    static gchar *groups[2] = { RSTTO_RECENT_FILES_GROUP , NULL };

    if (file == NULL) return FALSE;

    uri = g_file_get_uri (file);
    if(uri == NULL) return FALSE;

    file_info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
            0, NULL, NULL);
    if (file_info == NULL) return FALSE;

    recent_data = g_slice_new (GtkRecentData);
    recent_data->display_name = NULL;
    recent_data->description = NULL; 
    recent_data->mime_type = (gchar *) g_file_info_get_content_type (file_info);
    recent_data->app_name = RSTTO_RECENT_FILES_APP_NAME;
    recent_data->app_exec = g_strjoin(" ", g_get_prgname (), "%u", NULL);
    recent_data->groups = groups;
    recent_data->is_private = FALSE;

    gtk_recent_manager_add_full (gtk_recent_manager_get_default(), uri, recent_data);

    g_free (recent_data->app_exec);
    g_free (uri);
    g_object_unref (file_info);

    g_slice_free (GtkRecentData, recent_data);

    g_object_unref (file);
    return FALSE;
}

static void
cb_rstto_main_window_clear_private_data (
        GtkWidget *widget,
        RsttoMainWindow *window)
{
    GtkRecentFilter *recent_filter;
    gsize n_uris = 0;
    gchar **uris = NULL;
    guint i = 0;

    GtkWidget *dialog = rstto_privacy_dialog_new (GTK_WINDOW (window), window->priv->recent_manager);

    recent_filter = gtk_recent_filter_new();
    gtk_recent_filter_add_application (recent_filter, "ristretto");
    gtk_recent_chooser_add_filter(GTK_RECENT_CHOOSER(dialog), recent_filter);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK)
    {
        uris = gtk_recent_chooser_get_uris (GTK_RECENT_CHOOSER(dialog), &n_uris);
        for (i = 0; i < n_uris; ++i)
        {
            gtk_recent_manager_remove_item (window->priv->recent_manager, uris[i], NULL);
        }
    }

    gtk_widget_destroy (dialog);
}


static gboolean
key_press_event (
        GtkWidget *widget,
        GdkEventKey *event)
{
    GtkWindow *window = GTK_WINDOW ( widget );
    RsttoMainWindow *rstto_window = RSTTO_MAIN_WINDOW(widget);

    if ( FALSE == gtk_window_activate_key ( window, event ) )
    {
        switch(event->keyval)
        {
            case GDK_Up:
            case GDK_Left:
                rstto_image_list_iter_previous (rstto_window->priv->iter);
                break;
            case GDK_Right:
            case GDK_Down:
                rstto_image_list_iter_next (rstto_window->priv->iter);
                break;
        }
    }
    return TRUE;
}

static void
cb_rstto_merge_toolbars_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (user_data);
    rstto_main_window_update_buttons (window);
}

static void
cb_rstto_wrap_images_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (user_data);
    rstto_main_window_update_buttons (window);
}

static void
cb_rstto_desktop_type_changed (
        GObject *object,
        GParamSpec *pspec,
        gpointer user_data)
{
    RsttoMainWindow *window = RSTTO_MAIN_WINDOW (user_data);
    gchar *desktop_type = NULL;

    if (window->priv->wallpaper_manager)
    {
        g_object_unref(window->priv->wallpaper_manager);
        window->priv->wallpaper_manager = NULL;
    }

    desktop_type = rstto_settings_get_string_property (window->priv->settings_manager, "desktop-type");

    if (desktop_type)
    {
        if (!g_strcasecmp(desktop_type, "xfce"))
        {
            window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new();
        }

        if (!g_strcasecmp(desktop_type, "gnome"))
        {
            window->priv->wallpaper_manager = rstto_gnome_wallpaper_manager_new();
        }

        if (!g_strcasecmp(desktop_type, "none"))
        {
            window->priv->wallpaper_manager = NULL;
        }

        g_free (desktop_type);
        desktop_type = NULL;
    }
    else
    {
        /* Default to xfce */
        window->priv->wallpaper_manager = rstto_xfce_wallpaper_manager_new();
    }

    rstto_main_window_update_buttons (window);
}


gboolean
rstto_main_window_play_slideshow (RsttoMainWindow *window)
{
    GValue timeout = {0, };

    gtk_ui_manager_add_ui (window->priv->ui_manager,
                           window->priv->pause_merge_id,
                           "/main-menu/go-menu/placeholder-slideshow",
                           "pause",
                           "pause",
                           GTK_UI_MANAGER_MENUITEM,
                           FALSE);
    gtk_ui_manager_remove_ui (window->priv->ui_manager,
                              window->priv->play_merge_id);

    if ( TRUE == rstto_settings_get_boolean_property (window->priv->settings_manager, "merge-toolbars"))
    {
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_pause_merge_id,
                "/file-toolbar/placeholder-slideshow",
                "pause",
                "pause",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
        gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_play_merge_id);
    }
    else
    {
        gtk_ui_manager_add_ui (
                window->priv->ui_manager,
                window->priv->toolbar_pause_merge_id,
                "/navigation-toolbar/placeholder-slideshow",
                "pause",
                "pause",
                GTK_UI_MANAGER_TOOLITEM,
                FALSE);
        gtk_ui_manager_remove_ui (
                window->priv->ui_manager,
                window->priv->toolbar_play_merge_id);
    }


    g_value_init (&timeout, G_TYPE_UINT);
    g_object_get_property (G_OBJECT(window->priv->settings_manager), "slideshow-timeout", &timeout);

    window->priv->playing = TRUE;
    window->priv->play_timeout_id = g_timeout_add (g_value_get_uint (&timeout)*1000, (GSourceFunc)cb_rstto_main_window_play_slideshow, window);
}
