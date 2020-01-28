#pragma once

#include "window.h"

typedef enum _event {
    EVENT_WINDOW_MAP,
    EVENT_WINDOW_UNMAP,
    EVENT_WINDOW_CREATE,
    EVENT_WINDOW_DESTROY,
    EVENT_WINDOW_RESIZE,
    EVENT_DESKTOP_CHANGE,
    NUM_EVENTS,
    EVENT_UNKOWN
} event;

typedef void (*effect_func)(win *w, double progress);

typedef struct _effect {
    struct _effect *next;
    const char *name;
    effect_func func;
    double step;
} effect;

event get_event_from_name(const char *name);

effect *effect_find(const char *name);

Bool effect_new(const char *name, const char *function_name, double step);

void effect_set(wintype window_type, event e, effect *ef);

effect *effect_get(wintype window_type, event e);
