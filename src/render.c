#include "session.h"
#include "string.h"
#include "util.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xrender.h>

/* scales window down relative to its center
 * returns XRectangle area the window will take place after effect is applied
 * 
 * upscaling doesn't work maybe because damage is not added
 */
static XRectangle centered_scale(win *w) {
    if (w->scale > 1.0) // TODO for now only downscaling is supported so we force max scale to 1
        w->scale = 1.0;

    int x = w->attr.x;
    int y = w->attr.y;
    int width = w->attr.width + w->attr.border_width * 2;
    int heigt = w->attr.height + w->attr.border_width * 2;

    double offset_x = (width - (width * w->scale)) / 2.0; // use abs(wid - (wid * scale)) / 2.0 for upscale support
    double offset_y = (heigt - (heigt * w->scale)) / 2.0;

    // scale transformation matrix
    XTransform xform = {{{XDoubleToFixed(1.0), XDoubleToFixed(0.0), XDoubleToFixed(0.0)},
                         {XDoubleToFixed(0.0), XDoubleToFixed(1.0), XDoubleToFixed(0.0)},
                         {XDoubleToFixed(0.0), XDoubleToFixed(0.0), XDoubleToFixed(w->scale)}}};

    XRenderSetPictureFilter(s.dpy, w->picture, FilterBest, NULL, 0); // antialias scaled picture
    XRenderSetPictureTransform(s.dpy, w->picture, &xform);

    width *= w->scale;
    heigt *= w->scale;
    x += offset_x;
    y += offset_y;

    XRectangle r = {
        .x = x,
        .y = y,
        .width = width,
        .height = heigt};
    return r;
}

void add_damage(XserverRegion damage) {
    if (s.all_damage) {
        XFixesUnionRegion(s.dpy, s.all_damage, s.all_damage, damage);
        XFixesDestroyRegion(s.dpy, damage);
    } else
        s.all_damage = damage;
}

static Picture solid_picture(Bool argb, double a, double r, double g, double b) {
    Pixmap pixmap;
    Picture picture;
    XRenderPictureAttributes pa;
    XRenderColor c;

    pixmap = XCreatePixmap(s.dpy, s.root, 1, 1, argb ? 32 : 8);
    if (!pixmap)
        return None;

    pa.repeat = True;
    picture = XRenderCreatePicture(s.dpy, pixmap,
                                   XRenderFindStandardFormat(s.dpy, argb ? PictStandardARGB32 : PictStandardA8),
                                   CPRepeat,
                                   &pa);
    if (!picture) {
        XFreePixmap(s.dpy, pixmap);
        return None;
    }

    c.alpha = a * 0xffff;
    c.red = r * 0xffff;
    c.green = g * 0xffff;
    c.blue = b * 0xffff;
    XRenderFillRectangle(s.dpy, PictOpSrc, picture, &c, 0, 0, 1, 1);
    XFreePixmap(s.dpy, pixmap);
    return picture;
}

// render root window
// first get root window pixmap (to draw background image)
// if none, fill with arbitrary color set in alpha_colour variable of session struct
static Picture make_root_tile() {
    Picture picture;
    Atom actual_type;
    Pixmap pixmap;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop;
    Bool fill;
    XRenderPictureAttributes pa;

    pixmap = None;
    for (int p = 0; p < 2; p++) { // 2 is s.background_atoms length
        if (XGetWindowProperty(s.dpy, s.root, s.background_atoms[p],
                               0, 4, False, AnyPropertyType,
                               &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success &&
            actual_type == XInternAtom(s.dpy, "PIXMAP", False) && actual_format == 32 && nitems == 1) {
            memcpy(&pixmap, prop, 4);
            XFree(prop);
            fill = False;
            break;
        }
    }
    if (!pixmap) {
        pixmap = XCreatePixmap(s.dpy, s.root, 1, 1, DefaultDepth(s.dpy, s.screen));
        fill = True;
    }
    pa.repeat = True;
    picture = XRenderCreatePicture(s.dpy, pixmap,
                                   XRenderFindVisualFormat(s.dpy,
                                                           DefaultVisual(s.dpy, s.screen)),
                                   CPRepeat, &pa);
    if (fill) {
        XRenderColor c;

        c.red = c.green = c.blue = 0x8080;
        c.alpha = 0xffff;
        XRenderFillRectangle(s.dpy, PictOpSrc, picture, &c,
                             0, 0, 1, 1);
    }
    return picture;
}

static void paint_root() {
    if (!s.root_tile)
        s.root_tile = make_root_tile();

    XRenderComposite(s.dpy, PictOpSrc,
                     s.root_tile, None, s.root_buffer,
                     0, 0, 0, 0, 0, 0, s.root_width, s.root_height);
}

/*
 * region is None if window is not solid
 */
static void paint_window(win *w, XserverRegion region) {
    int x = w->attr.x;
    int y = w->attr.y;
    int wid = w->attr.width + w->attr.border_width * 2;
    int hei = w->attr.height + w->attr.border_width * 2;

    if (w->need_effect) {
        XRectangle r = {.x = x, .y = y, .width = wid, .height = hei};
        if (w->scale <= 1.0)
            r = centered_scale(w);

        r.x += w->offset_x;
        r.y += w->offset_y;

        x = r.x;
        y = r.y;
        wid = r.width;
        hei = r.height;

        // we crop all drawing outside of original window dimensions
        // TODO make possible to draw outside as well (for more effects like upscaling, ...)
        // but I will need to better understand how to clean drawing spillovers
        if (r.x < w->attr.x)
            r.x = w->attr.x;
        if (r.y < w->attr.y)
            r.y = w->attr.y;
        if (r.x + r.width > w->attr.x + w->attr.width)
            r.width = w->attr.width;
        if (r.y + r.height > w->attr.y + w->attr.height)
            r.height = w->attr.height;

        XFixesSetPictureClipRegion(s.dpy, s.root_buffer, 0, 0, XFixesCreateRegion(s.dpy, &r, 1));

        w->need_effect = False;
    }

    if (region) { // solid window
        XFixesSetPictureClipRegion(s.dpy, s.root_buffer, 0, 0, region);
        set_ignore(NextRequest(s.dpy));
        XFixesSubtractRegion(s.dpy, region, region, w->border_size);

        set_ignore(NextRequest(s.dpy));
        XRenderComposite(s.dpy, PictOpSrc, w->picture, None, s.root_buffer,
                         0, 0, 0, 0,
                         x, y, wid, hei);
    } else {
        XFixesIntersectRegion(s.dpy, w->border_clip, w->border_clip, w->border_size);
        XFixesSetPictureClipRegion(s.dpy, s.root_buffer, 0, 0, w->border_clip);

        // creates w->alpha_picture mask to apply window opacity
        if (w->opacity < 1.0 && !w->alpha_picture)
            w->alpha_picture = solid_picture(False, w->opacity, 0, 0, 0);

        set_ignore(NextRequest(s.dpy));
        XRenderComposite(s.dpy, PictOpOver, w->picture, w->alpha_picture, s.root_buffer,
                         0, 0, 0, 0,
                         x, y, wid, hei);
    }
}

void paint_all(XserverRegion region) {
    win *w;
    win *t = NULL;

    if (!region) {
        XRectangle r;
        r.x = 0;
        r.y = 0;
        r.width = s.root_width;
        r.height = s.root_height;
        region = XFixesCreateRegion(s.dpy, &r, 1);
    }

    if (!s.root_buffer) {
        Pixmap rootPixmap = XCreatePixmap(s.dpy, s.root, s.root_width, s.root_height,
                                          DefaultDepth(s.dpy, s.screen));
        s.root_buffer = XRenderCreatePicture(s.dpy, rootPixmap,
                                             XRenderFindVisualFormat(s.dpy,
                                                                     DefaultVisual(s.dpy, s.screen)),
                                             0, NULL);
        XFreePixmap(s.dpy, rootPixmap);
    }

    XFixesSetPictureClipRegion(s.dpy, s.root_picture, 0, 0, region);

    // draw solid windows into root_buffer
    for (w = s.managed_windows; w; w = w->next) {
        /* never painted, ignore it */
        if (!w->damaged)
            continue;
        /* if invisible, ignore it */
        if (w->attr.x + w->attr.width < 1 || w->attr.y + w->attr.height < 1 || w->attr.x >= s.root_width || w->attr.y >= s.root_height)
            continue;
        if (!w->picture) {
            XRenderPictureAttributes pa;
            XRenderPictFormat *format;
            Drawable draw = w->id;

            if (!w->pixmap)
                w->pixmap = XCompositeNameWindowPixmap(s.dpy, w->id);
            if (w->pixmap)
                draw = w->pixmap;

            format = XRenderFindVisualFormat(s.dpy, w->attr.visual);
            pa.subwindow_mode = IncludeInferiors;
            w->picture = XRenderCreatePicture(s.dpy, draw,
                                              format,
                                              CPSubwindowMode,
                                              &pa);
        }

        if (s.clip_changed) {
            if (w->border_size) {
                set_ignore(NextRequest(s.dpy));
                XFixesDestroyRegion(s.dpy, w->border_size);
                w->border_size = None;
            }
            if (w->extents) {
                XFixesDestroyRegion(s.dpy, w->extents);
                w->extents = None;
            }
            if (w->border_clip) {
                XFixesDestroyRegion(s.dpy, w->border_clip);
                w->border_clip = None;
            }
        }
        if (!w->border_size)
            w->border_size = border_size(w);
        if (!w->extents)
            w->extents = win_extents(w);
        if (w->mode == WINDOW_SOLID)
            paint_window(w, region);

        if (!w->border_clip) {
            w->border_clip = XFixesCreateRegion(s.dpy, NULL, 0);
            XFixesCopyRegion(s.dpy, w->border_clip, region);
        }
        w->prev_trans = t;
        t = w;
    }

    XFixesSetPictureClipRegion(s.dpy, s.root_buffer, 0, 0, region);
    paint_root();

    // draw non solid windows into root_buffer
    for (w = t; w; w = w->prev_trans) {
        XFixesSetPictureClipRegion(s.dpy, s.root_buffer, 0, 0, w->border_clip);

        if (w->mode == WINDOW_TRANS || w->mode == WINDOW_ARGB)
            paint_window(w, None);

        XFixesDestroyRegion(s.dpy, w->border_clip);
        w->border_clip = None;
    }
    XFixesDestroyRegion(s.dpy, region);
    if (s.root_buffer != s.root_picture) {
        XFixesSetPictureClipRegion(s.dpy, s.root_buffer, 0, 0, None);
        XRenderComposite(s.dpy, PictOpSrc, s.root_buffer, None, s.root_picture,
                         0, 0, 0, 0, 0, 0, s.root_width, s.root_height);
    }
}
