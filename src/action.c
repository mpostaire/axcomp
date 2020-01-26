#include "effect.h"
#include "session.h"
#include "util.h"
#include "window.h"
#include <sys/time.h>

typedef struct _action {
    struct _action *next;
    win *w;
    double progress; // in interval [0,1]
    double start;    // in interval [0,1]
    double end;      // in interval [0,1]
    double step;
    void (*callback)(win *w, Bool gone);
    effect effect;
    Bool gone;
} action;

static action *actions;
static int fade_delta = 3;
static int fade_time = 0;

static int get_time_in_milliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static action *action_find(win *w) {
    for (action *a = actions; a; a = a->next) {
        if (a->w == w)
            return a;
    }
    return NULL;
}

static void action_dequeue(action *a) {
    for (action **prev = &actions; *prev; prev = &(*prev)->next) {
        if (*prev == a) {
            *prev = a->next;
            if (a->callback)
                (*a->callback)(a->w, a->gone);
            free(a);
            break;
        }
    }
}

void action_cleanup(win *w) {
    action *a = action_find(w);
    if (a)
        action_dequeue(a);
}

static void action_enqueue(action *a) {
    if (!actions)
        fade_time = get_time_in_milliseconds() + fade_delta;
    a->next = actions;
    actions = a;
}

void action_set(win *w, double start, double end, double step, effect e, void (*callback)(win *w, Bool gone), Bool gone, Bool exec_callback) {
    action *a = action_find(w);
    if (!a) {
        a = malloc(sizeof(action));
        a->next = NULL;
        a->w = w;
        a->progress = start;
        action_enqueue(a);
    } else if (exec_callback && a->callback) {
        (*a->callback)(a->w, a->gone);
    }

    if (end < 0)
        end = 0;
    if (end > 1)
        end = 1;
    a->end = end;
    if (a->progress < end)
        a->step = step;
    else if (a->progress > end)
        a->step = -step;
    a->callback = callback;
    a->effect = e;
    a->gone = gone;

    (*a->effect)(w, a->progress);
}

int action_timeout(void) {
    if (!actions)
        return -1;
    int now = get_time_in_milliseconds();
    int delta = fade_time - now;
    if (delta < 0)
        delta = 0;
    return delta;
}

void action_run(void) {
    int now = get_time_in_milliseconds();
    action *next = actions;
    int steps;
    Bool need_dequeue;

    if (fade_time - now > 0)
        return;
    steps = 1 + (now - fade_time) / fade_delta;

    while (next) {
        action *a = next;
        win *w = a->w;
        next = a->next;
        a->progress += a->step * steps;
        if (a->progress >= 1)
            a->progress = 1;
        else if (a->progress < 0)
            a->progress = 0;

        (*a->effect)(w, a->progress);
        w->action_running = True;
        need_dequeue = False;
        if (a->step > 0) {
            if (a->progress >= a->end) {
                (*a->effect)(w, a->end);
                need_dequeue = True;
            }
        } else {
            if (a->progress <= a->end) {
                (*a->effect)(w, a->end);
                need_dequeue = True;
            }
        }
        determine_mode(w);
        // this is ugly
        w->mode = w->mode == WINDOW_SOLID ? WINDOW_ARGB : w->mode;

        /* Must do this last as it might destroy a->w in callbacks */
        if (need_dequeue) {
            determine_mode(w); // this is ugly
            action_dequeue(a);
            w->action_running = False;
        }
    }
    fade_time = now + fade_delta;
}
