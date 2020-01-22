#pragma once

#include "window.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <poll.h>

struct type_atoms {
    Atom type; // used to get the window type atom property

    Atom desktop;
    Atom dock;
    Atom toolbar;
    Atom menu;
    Atom util;
    Atom splash;
    Atom dialog;
    Atom normal;
};

struct session {
    Display *dpy;
    struct pollfd ufd;
    win *managed_windows;
    int screen;
    Window root;
    Picture root_picture;
    Picture root_buffer;
    Picture root_tile;
    XserverRegion all_damage;
    Bool clip_changed;
    int root_height, root_width;
    int xfixes_event, xfixes_error;
    int damage_event, damage_error;
    int composite_event, composite_error;
    int render_event, render_error;
    int xshape_event, xshape_error;
    int composite_opcode;

    Atom opacity_atom;
    Atom background_atoms[2];

    struct type_atoms window_types;
};

extern struct session s;

void session_loop(void);

void session_init(char *display);
