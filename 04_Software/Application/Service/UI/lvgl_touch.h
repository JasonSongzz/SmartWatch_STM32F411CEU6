#ifndef SERVICE_UI_LVGL_TOUCH_H
#define SERVICE_UI_LVGL_TOUCH_H

#include "lvgl.h"

/** Create an LVGL pointer input device backed by the touch Wrapper. */
lv_indev_t *lvgl_touch_init(lv_display_t *display);

#endif /* SERVICE_UI_LVGL_TOUCH_H */
