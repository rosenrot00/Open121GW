#ifndef EEVBLOG_BACKLIGHT_H
#define EEVBLOG_BACKLIGHT_H

#include <stdbool.h>

void backlight_init(void);
void backlight_set(bool enabled);
bool backlight_enabled(void);
bool backlight_toggle_persist(void);

#endif
