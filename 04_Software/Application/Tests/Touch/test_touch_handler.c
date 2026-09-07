#include "bsp_touch_handler.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    touch_status_t status;
    uint16_t x;
    uint16_t y;
    uint8_t gesture;
} scripted_sample_t;

static const scripted_sample_t *s_samples;
static size_t s_sample_count;
static size_t s_sample_index;

static touch_status_t fake_read(bsp_touch_driver_t *driver,
                                touch_point_t *point)
{
    const scripted_sample_t *sample;

    (void)driver;
    assert(s_sample_index < s_sample_count);
    sample = &s_samples[s_sample_index++];
    *point = (touch_point_t){
        .x = sample->x,
        .y = sample->y,
        .gesture = sample->gesture,
        .fingers = sample->status == TOUCH_OK ? 1U : 0U,
    };
    return sample->status;
}

static touch_status_t fake_get_info(const bsp_touch_driver_t *driver,
                                    touch_info_t *info)
{
    (void)driver;
    *info = (touch_info_t){
        .width = 240U,
        .height = 280U,
        .max_points = 1U,
    };
    return TOUCH_OK;
}

static touch_status_t fake_sleep(bsp_touch_driver_t *driver)
{
    driver->initialized = false;
    return TOUCH_OK;
}

static touch_status_t fake_wakeup(bsp_touch_driver_t *driver)
{
    driver->initialized = true;
    return TOUCH_OK;
}

touch_status_t cst816t_inst(
    bsp_touch_driver_t *touch, const touch_iic_interface_t *iic,
    const touch_yield_interface_t *yield,
    const touch_control_interface_t *control)
{
    (void)iic;
    (void)yield;
    (void)control;
    memset(touch, 0, sizeof(*touch));
    touch->width = 240U;
    touch->height = 280U;
    touch->initialized = true;
    touch->pf_read_point = fake_read;
    touch->pf_get_info = fake_get_info;
    touch->pf_sleep = fake_sleep;
    touch->pf_wakeup = fake_wakeup;
    return TOUCH_OK;
}

static void set_script(const scripted_sample_t *samples, size_t count)
{
    s_samples = samples;
    s_sample_count = count;
    s_sample_index = 0U;
}

static void init_handler(bsp_touch_handler_t *handler)
{
    static const touch_iic_interface_t iic;
    static const touch_control_interface_t control;
    static const touch_yield_interface_t yield;

    assert(touch_handler_init(handler, &iic, &control, &yield) == TOUCH_OK);
}

static void test_events_filter_and_release_hysteresis(void)
{
    const scripted_sample_t samples[] = {
        {TOUCH_OK, 100U, 100U, 0U},
        {TOUCH_OK, 101U, 99U, 0U},
        {TOUCH_OK, 230U, 250U, 0U},
        {TOUCH_NO_TOUCH, 0U, 0U, 0U},
        {TOUCH_NO_TOUCH, 0U, 0U, 0U},
        {TOUCH_NO_TOUCH, 0U, 0U, 0U},
    };
    bsp_touch_handler_t handler;
    touch_point_t point;

    init_handler(&handler);
    set_script(samples, sizeof(samples) / sizeof(samples[0]));

    assert(touch_handler_read(&handler, 10U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_DOWN && point.x == 100U &&
           point.y == 100U && point.fingers == 1U && point.sequence == 1U);

    assert(touch_handler_read(&handler, 20U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_MOVE && point.x == 100U &&
           point.y == 100U);

    assert(touch_handler_read(&handler, 30U, &point) == TOUCH_OK);
    assert(point.x <= 101U && point.y == 100U);

    assert(touch_handler_read(&handler, 40U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_MOVE && point.fingers == 1U);
    assert(touch_handler_read(&handler, 50U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_UP && point.fingers == 0U);
    assert(touch_handler_read(&handler, 60U, &point) == TOUCH_NO_TOUCH);
}

static void test_coordinate_mapping_and_out_of_range_rejection(void)
{
    const scripted_sample_t samples[] = {
        {TOUCH_OK, 10U, 20U, 0U},
        {TOUCH_NO_TOUCH, 0U, 0U, 0U},
        {TOUCH_NO_TOUCH, 0U, 0U, 0U},
        {TOUCH_OK, 9U, 20U, 0U},
    };
    bsp_touch_handler_t handler;
    touch_processing_config_t config;
    touch_point_t point;

    init_handler(&handler);
    assert(touch_handler_get_processing_config(&handler, &config) == TOUCH_OK);
    config.raw_x_min = 10U;
    config.raw_x_max = 110U;
    config.raw_y_min = 20U;
    config.raw_y_max = 220U;
    config.output_width = 201U;
    config.output_height = 101U;
    config.swap_xy = true;
    config.invert_x = true;
    config.invert_y = false;
    config.median_filter_enabled = false;
    assert(touch_handler_set_processing_config(&handler, &config) == TOUCH_OK);
    set_script(samples, sizeof(samples) / sizeof(samples[0]));

    assert(touch_handler_read(&handler, 1U, &point) == TOUCH_OK);
    assert(point.x == 0U && point.y == 100U);
    assert(touch_handler_read(&handler, 2U, &point) == TOUCH_OK);
    assert(touch_handler_read(&handler, 3U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_UP);
    assert(touch_handler_read(&handler, 4U, &point) == TOUCH_NO_TOUCH);
}

static void test_jump_confirmation(void)
{
    const scripted_sample_t samples[] = {
        {TOUCH_OK, 10U, 10U, 0U},
        {TOUCH_OK, 200U, 200U, 0U},
        {TOUCH_OK, 202U, 201U, 0U},
    };
    bsp_touch_handler_t handler;
    touch_processing_config_t config;
    touch_point_t point;

    init_handler(&handler);
    assert(touch_handler_get_processing_config(&handler, &config) == TOUCH_OK);
    config.median_filter_enabled = false;
    config.move_deadband_px = 0U;
    config.jump_threshold_px = 50U;
    config.jump_confirm_distance_px = 10U;
    config.slow_filter_alpha_q8 = 255U;
    config.fast_filter_alpha_q8 = 255U;
    assert(touch_handler_set_processing_config(&handler, &config) == TOUCH_OK);
    set_script(samples, sizeof(samples) / sizeof(samples[0]));

    assert(touch_handler_read(&handler, 10U, &point) == TOUCH_OK);
    assert(point.x == 10U && point.y == 10U);
    assert(touch_handler_read(&handler, 20U, &point) == TOUCH_OK);
    assert(point.x == 10U && point.y == 10U);
    assert(touch_handler_read(&handler, 30U, &point) == TOUCH_OK);
    assert(point.x >= 200U && point.y >= 199U);
}

static void test_config_validation_and_wakeup_reset(void)
{
    const scripted_sample_t samples[] = {
        {TOUCH_OK, 20U, 30U, 0U},
        {TOUCH_OK, 40U, 50U, 0U},
    };
    bsp_touch_handler_t handler;
    touch_processing_config_t config;
    touch_point_t point;

    init_handler(&handler);
    assert(touch_handler_get_processing_config(&handler, &config) == TOUCH_OK);
    config.raw_x_max = config.raw_x_min;
    assert(touch_handler_set_processing_config(&handler, &config) ==
           TOUCH_ERROR_PARAMETER);

    set_script(samples, sizeof(samples) / sizeof(samples[0]));
    assert(touch_handler_read(&handler, 1U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_DOWN);
    assert(touch_handler_sleep(&handler) == TOUCH_OK);
    assert(touch_handler_wakeup(&handler) == TOUCH_OK);
    assert(touch_handler_read(&handler, 2U, &point) == TOUCH_OK);
    assert(point.event == TOUCH_EVENT_DOWN);
}

int main(void)
{
    test_events_filter_and_release_hysteresis();
    test_coordinate_mapping_and_out_of_range_rejection();
    test_jump_confirmation();
    test_config_validation_and_wakeup_reset();
    puts("touch handler tests passed");
    return 0;
}
