#include "effect.h"
#include "session.h"
#include "util.h"
#include "window.h"

static void fade(win *w, double progress) {
    w->opacity = progress;
}

static void pop(win *w, double progress) {
    w->opacity = progress;
    double lower = 0.75;
    double upper = 1.0;
    w->scale = (progress * (upper - lower)) + lower;
}

static void slide_up(win *w, double progress) {
    w->offset_y = -((w->attr.height * progress) - w->attr.height);
}

static void slide_down(win *w, double progress) {
    w->offset_y = (w->attr.height * progress) - w->attr.height;
}

static void slide_left(win *w, double progress) {
    w->offset_x = -((w->attr.width * progress) - w->attr.width);
}

static void slide_right(win *w, double progress) {
    w->offset_x = (w->attr.width * progress) - w->attr.width;
}

// this is not that smart (when rules in config are implemented this will not be needed anymore)
static void smart_slide(win *w, double progress) {
    if (w->attr.width < w->attr.height) { // west or east
        int center_x = (w->attr.x + w->attr.width) / 2;
        if (center_x < s.root_width / 2) { // west
            slide_right(w, progress);
        } else { // east
            slide_left(w, progress);
        }
    } else { // north or south
        int center_y = (w->attr.y + w->attr.height) / 2;
        if (center_y < s.root_height / 2) { // north
            slide_down(w, progress);
        } else { // south
            slide_up(w, progress);
        }
    }
}

effect effect_select(wintype window_type) {
    switch (window_type) {
    case WINTYPE_DESKTOP:
        return NULL;
    case WINTYPE_DOCK:
        return smart_slide;
    case WINTYPE_TOOLBAR:
        return NULL;
    case WINTYPE_MENU:
        return NULL;
    case WINTYPE_UTILITY:
        return NULL;
    case WINTYPE_SPLASH:
        return NULL;
    case WINTYPE_DIALOG:
        return NULL;
    case WINTYPE_DROPDOWN_MENU:
        return NULL;
    case WINTYPE_POPUP_MENU:
        return fade;
    case WINTYPE_TOOLTIP:
        return fade;
    case WINTYPE_NOTIFICATION:
        return NULL;
    case WINTYPE_COMBO:
        return NULL;
    case WINTYPE_DND:
        return NULL;
    case WINTYPE_NORMAL:
        return pop;
    default:
        return NULL;
    }
}
