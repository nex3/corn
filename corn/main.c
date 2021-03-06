#include "config.h"

#include "gettext.h"
#include <locale.h>

#include "dbus.h"
#include "music.h"
#include "playlist.h"
#include "configuration.h"
#include "main.h"

#include <libgnomevfs/gnome-vfs.h>
#include <glib.h>
#include <gconf/gconf-client.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>

CornStatus main_status;

GStaticMutex main_mutex        = G_STATIC_MUTEX_INIT;

static GMainLoop * loop;

void main_quit() { g_main_loop_quit(loop); }

void signal_handler_quit(int signal) { main_quit(); }

void init_locale(void)
{
    if(!setlocale(LC_ALL, ""))
	g_warning("Couldn't set locale from environment.\n");
    bindtextdomain(PACKAGE_NAME, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE_NAME, "UTF-8");
    textdomain(PACKAGE_NAME);
}

void init_signals(void)
{
    sigset_t sigset;
    sigemptyset(&sigset);

    struct sigaction ignore_action = {
        .sa_handler = SIG_IGN,
        .sa_mask = sigset,
        .sa_flags = SA_NOCLDSTOP
    };

    sigaction(SIGHUP, &ignore_action, (struct sigaction *)NULL);

    sigaddset(&sigset, SIGTERM);
    sigaddset(&sigset, SIGINT);

    struct sigaction quit_action = {
        .sa_handler = signal_handler_quit,
        .sa_mask = sigset,
        .sa_flags = SA_NOCLDSTOP
    };

    sigaction(SIGTERM, &quit_action, (struct sigaction *)NULL);
    sigaction(SIGINT,  &quit_action, (struct sigaction *)NULL);

}

int main(int argc, char ** argv)
{
    main_status = CORN_STARTING;

    init_locale();

    init_signals();

    g_type_init();

    /* make sure the config directory exists */
    const gchar * dir = g_build_filename(g_get_user_config_dir(), PACKAGE, NULL);
    g_mkdir_with_parents(dir, S_IRWXU|S_IRWXG|S_IRWXO);

    GConfClient * gconf = gconf_client_get_default();
    gconf_client_add_dir(gconf, CORN_GCONF_ROOT, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
    gconf_client_notify_add(gconf, CORN_GCONF_ROOT, config_changed, NULL, NULL, NULL);

    int failed = 0;

    loop = g_main_loop_new(NULL, FALSE);
    if(!(failed = gnome_vfs_init() ? 0 : 5))
    {
        if(!(failed = music_init()))
        {
            if(!(failed = mpris_init()))
            {
                config_load(gconf);

                main_status = CORN_RUNNING;

                g_main_loop_run(loop);

                main_status = CORN_EXITING;

                g_static_mutex_lock(&main_mutex);
                config_save(gconf);
                g_static_mutex_unlock(&main_mutex);

                mpris_destroy();
            }
            music_destroy();
        }
        gnome_vfs_shutdown();
    }
    g_main_loop_unref(loop);
    g_object_unref(G_OBJECT(gconf));

    return failed;
}

