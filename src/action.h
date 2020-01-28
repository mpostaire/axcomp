#pragma once

#include "window.h"
#include "effect.h"

void action_cleanup(win *w);

void action_set(win *w, double start, double end, effect *e, void (*callback)(win *w, Bool gone), Bool gone, Bool exec_callback);

int action_timeout(void);

void action_run(void);
