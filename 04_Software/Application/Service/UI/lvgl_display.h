#ifndef SERVICE_UI_LVGL_DISPLAY_H
#define SERVICE_UI_LVGL_DISPLAY_H

#include "lvgl.h"

/** Create the LVGL display and bind its flush callback to the display Wrapper. */
lv_display_t *lvgl_display_init(void);

#endif /* SERVICE_UI_LVGL_DISPLAY_H */
