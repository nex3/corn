#include "config.h"

// BUG: i shouldn't have to include mpris-root before mpris-root-glue --
// dbus-glib-tool seems to be broken
#include "cpris-root.h"
#include "cpris-root-glue.h"
#include "mpris-root.h"
#include "mpris-root-glue.h"
#include "mpris-player.h"
#include "mpris-player-glue.h"
#include "mpris-tracklist.h"
#include "mpris-tracklist-glue.h"
#include "dbus.h"

#include <glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#define CORN_BUS_SERVICE "org.mpris.corn"

#define CORN_BUS_CROOT_PATH "/Corn"
#define CORN_BUS_ROOT_PATH "/"
#define CORN_BUS_PLAYER_PATH "/Player"
#define CORN_BUS_TRACKLIST_PATH "/TrackList"

#define CORN_BUS_INTERFACE "org.freedesktop.MediaPlayer"

static DBusGConnection * bus = NULL;

static gboolean mpris_register_objects(DBusGConnection *);

gboolean
mpris_init(void)
{
    dbus_g_thread_init();

    GError * error = NULL;
    if(!(bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error)))
    {
        g_printerr("Failed to open connection to D-BUS session bus (%s).\n",
                error->message);
        g_error_free(error);
        return FALSE;
    }

    DBusConnection * dbus_conn = dbus_g_connection_get_connection(bus);
    dbus_connection_set_exit_on_disconnect(dbus_conn, FALSE);
    // ok, so how do we get notified when the bus disconnects, to handle it
    // ourselves?

    return mpris_register_objects(bus);
}

void
mpris_destroy(void)
{
    if(bus)
        dbus_g_connection_unref(bus);
    bus = NULL;
}

static gboolean
mpris_register_objects(DBusGConnection * bus)
{
    DBusGProxy * bus_proxy = dbus_g_proxy_new_for_name(bus,
        "org.freedesktop.DBus",
        "/org/freedesktop/DBus",
        "org.freedesktop.DBus"
    );

    // TODO: it is possible to negotiate taking over a name.  this may or may
    // not be desirable.
    // http://dbus.freedesktop.org/doc/dbus-specification.html#message-bus-names

    GError * error = NULL;
    guint request_name_result;
    dbus_g_proxy_call(
        bus_proxy, "RequestName", &error,
        G_TYPE_STRING, CORN_BUS_SERVICE, G_TYPE_UINT, 0, G_TYPE_INVALID,
        G_TYPE_UINT, &request_name_result, G_TYPE_INVALID
    );

    if(error)
    {
        g_printerr("Error during RequestName proxy call (%s).\n", error->message);
        g_error_free(error);
        return FALSE;
    }

    if(!request_name_result)
    {
        g_printerr("Failed to acquire %s service.\n", CORN_BUS_SERVICE);
        return FALSE;
    }

    CprisRoot * croot = g_object_new(CORN_TYPE_CPRIS_ROOT, NULL);
    MprisRoot * root = g_object_new(CORN_TYPE_MPRIS_ROOT, NULL);
    MprisPlayer * player = g_object_new(CORN_TYPE_MPRIS_PLAYER, NULL);
    MprisTrackList * tracklist = g_object_new(CORN_TYPE_MPRIS_TRACKLIST, NULL);

    // these object info thingies are generated by dbus-glib-tool from the xml
    // file and put into the glue header.
    dbus_g_object_type_install_info(CORN_TYPE_CPRIS_ROOT, &dbus_glib_cpris_root_object_info);
    dbus_g_object_type_install_info(CORN_TYPE_MPRIS_ROOT, &dbus_glib_mpris_root_object_info);
    dbus_g_object_type_install_info(CORN_TYPE_MPRIS_PLAYER, &dbus_glib_mpris_player_object_info);
    dbus_g_object_type_install_info(CORN_TYPE_MPRIS_TRACKLIST, &dbus_glib_mpris_tracklist_object_info);

    // error check!
    dbus_g_connection_register_g_object(bus, CORN_BUS_CROOT_PATH, G_OBJECT(croot));
    dbus_g_connection_register_g_object(bus, CORN_BUS_ROOT_PATH, G_OBJECT(root));
    dbus_g_connection_register_g_object(bus, CORN_BUS_PLAYER_PATH, G_OBJECT(player));
    dbus_g_connection_register_g_object(bus, CORN_BUS_TRACKLIST_PATH, G_OBJECT(tracklist));

    return TRUE;
}

