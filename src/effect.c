#include "effect.h"
#include "session.h"
#include "util.h"
#include "window.h"
#include <string.h>

// TODO when an effect is replaced by another, we should clean all effect related variables

static effect *effect_dispatch_table[NUM_WINTYPES][NUM_EVENT_EFFECTS] = {{NULL}};
static effect *effects;

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

effect *effect_get(wintype window_type, event_effect event) {
    return effect_dispatch_table[window_type][event];
}

void effect_set(wintype window_type, event_effect event, effect *e) {
    effect_dispatch_table[window_type][event] = e;
}

static const char *event_effect_names[] = {"map-effect", "unmap-effect", "create-effect", "destroy-effect",
                                    "maximize-effect", "move-effect", "desktop-change-effect"};
const char *get_event_effect_name(event_effect effect) {
    if (effect >= NUM_EVENT_EFFECTS)
        return NULL;
    return event_effect_names[effect];
}

static const effect_func effect_funcs[] = {fade, pop, smart_slide};
static const char *effect_funcs_names[] = {"fade", "pop", "slide"};
effect_func get_effect_func_from_name(const char *name) {
    for (int i = 0; i < 3; i++)
        if (strcmp(name, effect_funcs_names[i]) == 0)
            return effect_funcs[i];
    return NULL;
}

effect *effect_find(const char *name) {
    for (effect *e = effects; e; e = e->next) {
        if (strcmp(e->name, name) == 0)
            return e;
    }
    return NULL;
}

void effect_new(const char *name, const char *function_name, double step) {
    if (effect_find(name))
        return;
    effect *e = ecalloc(1, sizeof(effect));

    e->func = get_effect_func_from_name(function_name);
    if (!e->func) {
        free(e);
        return;
    }
    e->name = name;
    e->step = step;

    e->next = effects;
    effects = e;

    return;
}
