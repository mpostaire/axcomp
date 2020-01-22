#include "session.h"
#include <X11/extensions/Xdamage.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(const char *program, Bool failed) {
    fprintf(stderr, "usage: %s [options]\n%s\n", program,
            "Options:\n"
            "   -d display\n"
            "      Specifies which display should be managed.\n"
            "   -d help\n"
            "      Show this message.\n");

    exit(failed ? EXIT_FAILURE : EXIT_SUCCESS);
}

// TODO use composite overlay window to paint ?

// TODO use shared memory extension for huge performance boost (Xshm)

// TODO features : shadows, fade in fade out, pop in pop out, gnome like maximize/minimize animation
// dock type windows appear gliding from the side (make funtion to detect wich side the dock is likely to be attached),
// detection of desktop change for special effects (current desktop var in memory and when a client is managed, we keep in memory its desktop)
// this only works for ewmh or icccm (check wich one) WMs
// when a desktop change (root window desktop prop change) we apply effects to clients from prev and new desktop
// we can also make special effects when a desktop change of a non root window (window change desktop)
// this can cause conflicts with awesomewm combinable desktop (or tags) system but this is not a behaviour most WMs use
// because we can't get all desktops associated with a window but only one
// one way to fix this in awesome is to use setproperty() from xproperties in awesome config to set a new property that is a list of desktops
// but this is a hacky way. (after reading doc it this is an incomplete implementation so its better to use xprop command spawned with easy_async)

// TODO disable when fullscreen app detected (for gaming performances)
// TODO vsync ?

// TODO valgrind test

// TODO check this fork fixes and try to add them http://git.openbox.org/?p=dana/xcompmgr.git;a=summary

// TODO adapt fade structure and code to accept effect functions (make fade code an action in time linked to an effect) like libgdx

int main(int argc, char **argv) {
    char *display = NULL;
    char o;
    while ((o = getopt(argc, argv, "hd:")) != -1) {
        switch (o) {
        case 'h':
            usage(argv[0], False);
            break;
        case 'd':
            display = optarg;
            break;
        default:
            usage(argv[0], True);
            break;
        }
    }

    session_init(display);

    session_loop();

    return EXIT_SUCCESS;
}
