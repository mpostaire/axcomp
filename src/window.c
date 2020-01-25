#include "window.h"
#include "action.h"
#include "effect.h"
#include "render.h"
#include "session.h"
#include "util.h"
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <stdlib.h>
#include <string.h>

static double fade_in_step = 0.03;
static double fade_out_step = 0.03;

win *find_win(Window id) {
    for (win *w = s.managed_windows; w; w = w->next)
        if (w->id == id)
            return w;
    return NULL;
}

XserverRegion win_extents(win *w) {
    XRectangle r;

    COPY_AREA(&r, &w->attr);
    r.width += w->attr.border_width * 2;
    r.height += w->attr.border_width * 2;

    return XFixesCreateRegion(s.dpy, &r, 1);
}

XserverRegion border_size(win *w) {
    XserverRegion border;
    /*
     * if window doesn't exist anymore,  this will generate an error
     * as well as not generate a region.  Perhaps a better XFixes
     * architecture would be to have a request that copies instead
     * of creates, that way you'd just end up with an empty region
     * instead of an invalid XID.
     */
    set_ignore(NextRequest(s.dpy));
    border = XFixesCreateRegionFromWindow(s.dpy, w->id, WindowRegionBounding);
    /* translate this */
    set_ignore(NextRequest(s.dpy));
    XFixesTranslateRegion(s.dpy, border,
                          w->attr.x + w->attr.border_width,
                          w->attr.y + w->attr.border_width);
    return border;
}

void map_win(Window id) {
    win *w = find_win(id);
    if (!w)
        return;

    w->attr.map_state = IsViewable;

    // This needs to be here or else we lose transparency messages
    XSelectInput(s.dpy, id, PropertyChangeMask);

    // This needs to be here since we don't get PropertyNotify when unmapped
    w->opacity = get_opacity_prop(w, OPAQUE);
    determine_mode(w);

    w->damaged = False;

    effect e;
    if ((e = effect_select(w->window_type)))
        action_set(w, 0, get_opacity_prop(w, 1.0), fade_in_step, e, NULL, False, True);
}

void finish_unmap_win(win *w) {
    w->damaged = False;

    if (w->extents != None) {
        add_damage(w->extents); // destroys region
        w->extents = None;
    }

    if (w->pixmap) {
        XFreePixmap(s.dpy, w->pixmap);
        w->pixmap = None;
    }

    if (w->picture) {
        set_ignore(NextRequest(s.dpy));
        XRenderFreePicture(s.dpy, w->picture);
        w->picture = None;
    }

    // don't care about properties anymore
    set_ignore(NextRequest(s.dpy));
    XSelectInput(s.dpy, w->id, 0);

    if (w->border_size) {
        set_ignore(NextRequest(s.dpy));
        XFixesDestroyRegion(s.dpy, w->border_size);
        w->border_size = None;
    }
    if (w->border_clip) {
        XFixesDestroyRegion(s.dpy, w->border_clip);
        w->border_clip = None;
    }

    s.clip_changed = True;
}

static void unmap_callback(win *w, Bool gone) {
    finish_unmap_win(w);
}

void unmap_win(Window id) {
    win *w = find_win(id);
    if (!w)
        return;
    w->attr.map_state = IsUnmapped;
    effect e;
    if ((e = effect_select(w->window_type)) && w->pixmap)
        action_set(w, w->opacity * 1.0 / OPAQUE, 0.0, fade_out_step, e, unmap_callback, False, False);
    else
        finish_unmap_win(w);
}

/* Get the opacity prop from window
   not found: def
   otherwise the value
 */
unsigned int get_opacity_prop(win *w, unsigned int def) {
    Atom actual;
    int format;
    unsigned long n, left;

    unsigned char *data;
    int result = XGetWindowProperty(s.dpy, w->id, s.opacity_atom, 0L, 1L, False,
                                    XA_CARDINAL, &actual, &format,
                                    &n, &left, &data);
    if (result == Success && data != NULL) {
        unsigned int i = *(unsigned int *) data;
        XFree((void *) data);
        return i;
    }
    return def;
}

void determine_mode(win *w) {
    int mode;
    XRenderPictFormat *format;

    if (w->alpha_picture) {
        XRenderFreePicture(s.dpy, w->alpha_picture);
        w->alpha_picture = None;
    }

    if (w->attr.class == InputOnly) {
        format = NULL;
    } else {
        format = XRenderFindVisualFormat(s.dpy, w->attr.visual);
    }

    if (format && format->type == PictTypeDirect && format->direct.alphaMask) {
        mode = WINDOW_ARGB;
    } else if (w->opacity != OPAQUE) {
        mode = WINDOW_TRANS;
    } else {
        mode = WINDOW_SOLID;
    }
    w->mode = mode;
    if (w->extents) {
        XserverRegion damage;
        damage = XFixesCreateRegion(s.dpy, NULL, 0);
        XFixesCopyRegion(s.dpy, damage, w->extents);
        add_damage(damage);
    }
}

static wintype determine_wintype(win *w) {
    Atom actual;
    int format;
    unsigned long n, left;

    unsigned char *data;
    // s.wintype_atoms[NUM_WINTYPES] is the _NET_WM_WINDOW_TYPE atom used to query a window type
    int result = XGetWindowProperty(s.dpy, w->id, s.wintype_atoms[NUM_WINTYPES], 0L, 1L, False,
                                    XA_ATOM, &actual, &format,
                                    &n, &left, &data);

    if (result == Success && data != (unsigned char *) None) {
        Atom a = *(Atom *) data;
        XFree((void *) data);

        for (int i = 0; i < NUM_WINTYPES; i++)
            if (s.wintype_atoms[i] == a)
                return i;
    }

    Window transient_for;
    result = XGetTransientForHint(s.dpy, w->id, &transient_for);
    if (w->attr.override_redirect || result)
        return WINTYPE_NORMAL;
    return WINTYPE_DIALOG;
}

// en gros ça utilise une liste de window comme un stack
// si prev null alors ça add normalement au stack
// sinon ça insère avant la window prev
void add_win(Window id) {
    win *w = ecalloc(1, sizeof(win));
    w->id = id;
    set_ignore(NextRequest(s.dpy));
    if (!XGetWindowAttributes(s.dpy, id, &w->attr)) {
        free(w);
        return;
    }

    w->shaped = False;
    w->shape_bounds.x = w->attr.x;
    w->shape_bounds.y = w->attr.y;
    w->shape_bounds.width = w->attr.width;
    w->shape_bounds.height = w->attr.height;

    w->damaged = False;
    w->pixmap = None;
    w->picture = None;

    w->damage = w->attr.class == InputOnly ? None : XDamageCreate(s.dpy, id, XDamageReportNonEmpty);

    w->alpha_picture = None;
    w->border_size = None;
    w->extents = None;
    w->opacity = OPAQUE;
    w->border_clip = None;

    w->scale = 1.0;
    w->offset_x = 0;
    w->offset_y = 0;
    w->need_effect = False;

    w->prev_trans = NULL;

    w->window_type = determine_wintype(w);

    w->next = s.managed_windows;
    s.managed_windows = w;

    if (w->attr.map_state == IsViewable)
        map_win(id);
}

void restack_win(win *w, Window new_above) {
    Window old_above = w->next ? w->next->id : None;

    if (old_above != new_above) {
        win **prev;

        /* unhook */
        for (prev = &s.managed_windows; *prev; prev = &(*prev)->next)
            if ((*prev) == w)
                break;
        *prev = w->next;

        /* rehook */
        for (prev = &s.managed_windows; *prev; prev = &(*prev)->next) {
            if ((*prev)->id == new_above)
                break;
        }
        w->next = *prev;
        *prev = w;
    }
}

void configure_win(XConfigureEvent *ce) {
    win *w = find_win(ce->window);
    XserverRegion damage = None;

    if (!w) {
        if (ce->window == s.root) {
            if (s.root_buffer) {
                XRenderFreePicture(s.dpy, s.root_buffer);
                s.root_buffer = None;
            }
            s.root_width = ce->width;
            s.root_height = ce->height;
        }
        return;
    }

    { // TODO what are these curly braces for...
        damage = XFixesCreateRegion(s.dpy, NULL, 0);
        if (w->extents != None)
            XFixesCopyRegion(s.dpy, damage, w->extents);
    }

    w->shape_bounds.x -= w->attr.x;
    w->shape_bounds.y -= w->attr.y;

    if (w->attr.width != ce->width || w->attr.height != ce->height) {
        if (w->pixmap) {
            XFreePixmap(s.dpy, w->pixmap);
            w->pixmap = None;
            if (w->picture) {
                XRenderFreePicture(s.dpy, w->picture);
                w->picture = None;
            }
        }
    }

    COPY_AREA(&w->attr, ce);
    w->attr.border_width = ce->border_width;
    w->attr.override_redirect = ce->override_redirect;
    restack_win(w, ce->above);
    if (damage) {
        XserverRegion extents = win_extents(w);
        XFixesUnionRegion(s.dpy, damage, damage, extents);
        XFixesDestroyRegion(s.dpy, extents);
        add_damage(damage);
    }
    w->shape_bounds.x += w->attr.x;
    w->shape_bounds.y += w->attr.y;
    if (!w->shaped) {
        w->shape_bounds.width = w->attr.width;
        w->shape_bounds.height = w->attr.height;
    }

    s.clip_changed = True;
}

void circulate_win(XCirculateEvent *ce) {
    win *w = find_win(ce->window);
    Window new_above;

    if (!w)
        return;

    if (ce->place == PlaceOnTop)
        new_above = s.managed_windows->id;
    else
        new_above = None;
    restack_win(w, new_above);
    s.clip_changed = True;
}

static void finish_destroy_win(Window id, Bool gone) {
    win **prev, *w;
    for (prev = &s.managed_windows; (w = *prev); prev = &w->next) {
        if (w->id == id) {
            if (gone)
                finish_unmap_win(w);
            *prev = w->next;
            if (w->picture) {
                set_ignore(NextRequest(s.dpy));
                XRenderFreePicture(s.dpy, w->picture);
                w->picture = None;
            }
            if (w->alpha_picture) {
                XRenderFreePicture(s.dpy, w->alpha_picture);
                w->alpha_picture = None;
            }
            if (w->damage != None) {
                set_ignore(NextRequest(s.dpy));
                XDamageDestroy(s.dpy, w->damage);
                w->damage = None;
            }
            action_cleanup(w);
            free(w);
            break;
        }
    }
}

static void destroy_callback(win *w, Bool gone) {
    finish_destroy_win(w->id, gone);
}

void destroy_win(Window id, Bool gone) {
    win *w = find_win(id);
    effect e;
    if (w && (e = effect_select(w->window_type)) && w->pixmap)
        action_set(w, w->opacity * 1.0 / OPAQUE, 0.0, fade_out_step, e, destroy_callback, gone, False);
    else
        finish_destroy_win(id, gone);
}

void damage_win(XDamageNotifyEvent *de) {
    XserverRegion parts;
    win *w = find_win(de->drawable);
    if (!w)
        return;

    if (!w->damaged) {
        parts = win_extents(w);
        set_ignore(NextRequest(s.dpy));
        XDamageSubtract(s.dpy, w->damage, None, None);
    } else {
        parts = XFixesCreateRegion(s.dpy, NULL, 0);
        set_ignore(NextRequest(s.dpy));
        XDamageSubtract(s.dpy, w->damage, None, parts);
        XFixesTranslateRegion(s.dpy, parts,
                              w->attr.x + w->attr.border_width,
                              w->attr.y + w->attr.border_width);
    }
    add_damage(parts);
    w->damaged = True;
}

// TODO there is no support for this in my fork
void shape_win(XShapeEvent *se) {
    win *w = find_win(se->window);

    if (!w)
        return;

    if (se->kind == ShapeClip || se->kind == ShapeBounding) {
        XserverRegion region0;
        XserverRegion region1;

        s.clip_changed = True;

        region0 = XFixesCreateRegion(s.dpy, &w->shape_bounds, 1);

        if (se->shaped == True) {
            w->shaped = True;
            w->shape_bounds.x = w->attr.x + se->x;
            w->shape_bounds.y = w->attr.y + se->y;
            w->shape_bounds.width = se->width;
            w->shape_bounds.height = se->height;
        } else {
            w->shaped = False;
            w->shape_bounds.x = w->attr.x;
            w->shape_bounds.y = w->attr.y;
            w->shape_bounds.width = w->attr.width;
            w->shape_bounds.height = w->attr.height;
        }

        region1 = XFixesCreateRegion(s.dpy, &w->shape_bounds, 1);
        XFixesUnionRegion(s.dpy, region0, region0, region1);
        XFixesDestroyRegion(s.dpy, region1);

        /* ask for repaint of the old and new region */
        paint_all(region0);
    }
}
