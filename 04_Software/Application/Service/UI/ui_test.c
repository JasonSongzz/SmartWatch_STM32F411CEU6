#include "ui_test.h"

#include "lvgl.h"

static void ui_test_button_event(lv_event_t *event)
{
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(event);
    if (label != NULL) lv_label_set_text(label, "Pressed");
}

void ui_test_create(void)
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *title;
    lv_obj_t *status;
    lv_obj_t *button;
    lv_obj_t *button_label;

    lv_obj_set_style_bg_color(screen, lv_color_hex(0x102A43), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    title = lv_label_create(screen);
    lv_label_set_text(title, "Smart Watch");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    status = lv_label_create(screen);
    lv_label_set_text(status, "LVGL UI Ready");
    lv_obj_set_style_text_color(status, lv_color_hex(0xB9D6F2), 0);
    lv_obj_align(status, LV_ALIGN_CENTER, 0, -32);

    button = lv_button_create(screen);
    lv_obj_set_size(button, 150, 54);
    lv_obj_align(button, LV_ALIGN_CENTER, 0, 38);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2F80ED), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1C5DA8),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    button_label = lv_label_create(button);
    lv_label_set_text(button_label, "Touch Button");
    lv_obj_set_style_text_color(button_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(button_label);
    lv_obj_add_event_cb(button, ui_test_button_event, LV_EVENT_CLICKED, status);
}
