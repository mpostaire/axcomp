#pragma once

#include "window.h"

typedef void (*effect)(win *w, double progress);

effect effect_select(wintype window_type);
