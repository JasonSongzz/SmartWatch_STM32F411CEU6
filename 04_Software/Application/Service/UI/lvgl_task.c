#include "lvgl_task.h"

#include "lvgl.h"
#include "lvgl_port_display.h"
#include "ui_test.h"
#include "osal.h"
#include "iwdg.h"

void lvgl_task_entry(void *argument)
{
    (void)argument;
    (void)HAL_IWDG_Refresh(&hiwdg);
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    if (lvgl_port_display_init() != NULL)
    {
        ui_test_create();
    }
    for (;;)
    {
        (void)lv_timer_handler();
        /* UI is system-critical during bring-up; keep IWDG alive only here. */
        (void)HAL_IWDG_Refresh(&hiwdg);
        osal_task_delay_ms(5U);
    }
}
