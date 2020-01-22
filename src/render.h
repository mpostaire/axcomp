#pragma once

#include <X11/extensions/Xdamage.h>

void effect_opacity(win *w, double opacity);

void add_damage(XserverRegion damage);

void paint_all(XserverRegion region);
