#pragma once

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#define WINDOW_SOLID 0
#define WINDOW_TRANS 1
#define WINDOW_ARGB 2

typedef enum _wintype {
    WINTYPE_DESKTOP,
    WINTYPE_DOCK,
    WINTYPE_TOOLBAR, // toolbar detached from main window
    WINTYPE_MENU,    // pinnable menu detached from main window
    WINTYPE_UTILITY, // small persistent utility window (e.g. palette or toolbox)
    WINTYPE_SPLASH,  // splash window when an app is starting up
    WINTYPE_DIALOG,
    WINTYPE_DROPDOWN_MENU, // menu spawned from a menubar
    WINTYPE_POPUP_MENU,    // menu spawned from a right click
    WINTYPE_TOOLTIP,
    WINTYPE_NOTIFICATION,
    WINTYPE_COMBO, // window popped up by combo boxes
    WINTYPE_DND,   // window being dragged
    WINTYPE_NORMAL,
    NUM_WINTYPES
} wintype;

typedef struct _win {
    struct _win *next;
    Window id;
    Pixmap pixmap;
    XWindowAttributes attr;

    int mode;
    Bool damaged;
    Damage damage;
    Picture picture;
    Picture alpha_picture;
    XserverRegion border_size;
    XserverRegion extents;
    wintype window_type;
    Bool shaped;
    XRectangle shape_bounds;

    double opacity;
    double scale;
    int offset_x;
    int offset_y;
    Bool need_effect; // used to apply effects when painting a window

    /* for drawing translucent windows */
    XserverRegion border_clip;
    struct _win *prev_trans;
} win;

win *find_win(Window id);

XserverRegion win_extents(win *w);

XserverRegion border_size(win *w);

void map_win(Window id);

void finish_unmap_win(win *w);

void unmap_win(Window id);

/* Get the opacity prop from window
   not found: default
   otherwise the value
 */
double get_opacity_prop(win *w, double def);

void determine_mode(win *w);

// en gros ça utilise une liste de window comme un stack
// si prev null alors ça add normalement au stack
// sinon ça insère avant la window prev
void add_win(Window id);

void restack_win(win *w, Window new_above);

void configure_win(XConfigureEvent *ce);

void circulate_win(XCirculateEvent *ce);

void destroy_win(Window id, Bool gone);

void damage_win(XDamageNotifyEvent *de);

void shape_win(XShapeEvent *se);
