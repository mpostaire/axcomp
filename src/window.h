#pragma once

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xrender.h>
#include <X11/extensions/shape.h>

#define WINDOW_SOLID 0
#define WINDOW_TRANS 1
#define WINDOW_ARGB 2

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
    unsigned int opacity;
    Atom window_type;
    Bool shaped;
    XRectangle shape_bounds;

    double scale;
    Bool need_scaling; // used to apply scaling transformation matrix only when needed

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
unsigned int get_opacity_prop(win *w, unsigned int def);

/* determine mode for window all in one place.
   Future might check for menu flag and other cool things
*/
Atom get_wintype_prop(Window w);

void determine_mode(win *w);

Atom determine_wintype(Window w);

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
