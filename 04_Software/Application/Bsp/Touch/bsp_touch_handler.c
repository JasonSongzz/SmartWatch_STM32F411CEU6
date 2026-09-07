#include "bsp_touch_handler.h"

#include <stddef.h>
#include <string.h>

#define TOUCH_FILTER_Q8_ONE              (256L)
#define TOUCH_DEFAULT_DEADBAND_PX        (2U)
#define TOUCH_DEFAULT_FAST_SPEED_PX_S    (600U)
#define TOUCH_DEFAULT_JUMP_PX            (80U)
#define TOUCH_DEFAULT_JUMP_CONFIRM_PX    (24U)
#define TOUCH_DEFAULT_SLOW_ALPHA_Q8      (64U)
#define TOUCH_DEFAULT_FAST_ALPHA_Q8      (192U)
#define TOUCH_DEFAULT_PRESS_SAMPLES      (1U)
#define TOUCH_DEFAULT_RELEASE_SAMPLES    (2U)

static uint16_t touch_abs_diff(uint16_t lhs, uint16_t rhs)
{
    return lhs >= rhs ? (uint16_t)(lhs - rhs) : (uint16_t)(rhs - lhs);
}

static uint16_t touch_median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b)
    {
        uint16_t temporary = a;
        a = b;
        b = temporary;
    }
    if (b > c)
    {
        uint16_t temporary = b;
        b = c;
        c = temporary;
    }
    if (a > b) b = a;
    return b;
}

static void touch_handler_reset_runtime(bsp_touch_handler_t *handler)
{
    handler->last_x = 0U;
    handler->last_y = 0U;
    handler->jump_x = 0U;
    handler->jump_y = 0U;
    handler->filtered_x_q8 = 0;
    handler->filtered_y_q8 = 0;
    handler->last_timestamp_ms = 0U;
    handler->history_count = 0U;
    handler->history_index = 0U;
    handler->press_count = 0U;
    handler->release_count = 0U;
    handler->pressed = false;
    handler->filter_seeded = false;
    handler->jump_pending = false;
    memset(handler->history_x, 0, sizeof(handler->history_x));
    memset(handler->history_y, 0, sizeof(handler->history_y));
}

static bool touch_processing_config_is_valid(
    const touch_processing_config_t *config)
{
    if (config == NULL || config->raw_x_max <= config->raw_x_min ||
        config->raw_y_max <= config->raw_y_min ||
        config->output_width == 0U || config->output_height == 0U ||
        config->slow_filter_alpha_q8 == 0U ||
        config->fast_filter_alpha_q8 == 0U ||
        config->press_debounce_samples == 0U ||
        config->release_debounce_samples == 0U)
        return false;

    return config->slow_filter_alpha_q8 <= config->fast_filter_alpha_q8;
}

static touch_status_t touch_map_point(
    const touch_processing_config_t *config, const touch_point_t *raw,
    uint16_t *mapped_x, uint16_t *mapped_y)
{
    uint32_t x_numerator;
    uint32_t y_numerator;
    uint32_t x_denominator;
    uint32_t y_denominator;
    uint32_t x;
    uint32_t y;

    if (raw->x < config->raw_x_min || raw->x > config->raw_x_max ||
        raw->y < config->raw_y_min || raw->y > config->raw_y_max)
        return TOUCH_NO_TOUCH;

    x_denominator = (uint32_t)config->raw_x_max - config->raw_x_min;
    y_denominator = (uint32_t)config->raw_y_max - config->raw_y_min;
    x_numerator = (uint32_t)raw->x - config->raw_x_min;
    y_numerator = (uint32_t)raw->y - config->raw_y_min;

    if (config->invert_x) x_numerator = x_denominator - x_numerator;
    if (config->invert_y) y_numerator = y_denominator - y_numerator;

    if (config->swap_xy)
    {
        x = y_numerator * (config->output_width - 1U) / y_denominator;
        y = x_numerator * (config->output_height - 1U) / x_denominator;
    }
    else
    {
        x = x_numerator * (config->output_width - 1U) / x_denominator;
        y = y_numerator * (config->output_height - 1U) / y_denominator;
    }

    *mapped_x = (uint16_t)x;
    *mapped_y = (uint16_t)y;
    return TOUCH_OK;
}

static void touch_push_history(bsp_touch_handler_t *handler,
                               uint16_t x, uint16_t y,
                               uint16_t *filtered_x,
                               uint16_t *filtered_y)
{
    handler->history_x[handler->history_index] = x;
    handler->history_y[handler->history_index] = y;
    handler->history_index = (uint8_t)((handler->history_index + 1U) % 3U);
    if (handler->history_count < 3U) handler->history_count++;

    *filtered_x = x;
    *filtered_y = y;
    if (handler->config.median_filter_enabled && handler->history_count == 3U)
    {
        *filtered_x = touch_median3(handler->history_x[0],
                                    handler->history_x[1],
                                    handler->history_x[2]);
        *filtered_y = touch_median3(handler->history_y[0],
                                    handler->history_y[1],
                                    handler->history_y[2]);
    }
}

static bool touch_jump_is_accepted(bsp_touch_handler_t *handler,
                                   uint16_t x, uint16_t y)
{
    uint32_t distance;
    uint32_t confirmation_distance;

    if (!handler->pressed || handler->config.jump_threshold_px == 0U)
        return true;

    distance = (uint32_t)touch_abs_diff(x, handler->last_x) +
               touch_abs_diff(y, handler->last_y);
    if (distance <= handler->config.jump_threshold_px)
    {
        handler->jump_pending = false;
        return true;
    }

    confirmation_distance = (uint32_t)touch_abs_diff(x, handler->jump_x) +
                            touch_abs_diff(y, handler->jump_y);
    if (handler->jump_pending &&
        confirmation_distance <= handler->config.jump_confirm_distance_px)
    {
        handler->jump_pending = false;
        return true;
    }

    handler->jump_x = x;
    handler->jump_y = y;
    handler->jump_pending = true;
    return false;
}

static void touch_apply_iir(bsp_touch_handler_t *handler,
                            uint32_t timestamp_ms,
                            uint16_t x, uint16_t y,
                            uint16_t *output_x, uint16_t *output_y)
{
    uint32_t elapsed_ms;
    uint32_t distance;
    uint32_t speed;
    int32_t alpha;

    if (!handler->filter_seeded)
    {
        handler->filtered_x_q8 = (int32_t)x * TOUCH_FILTER_Q8_ONE;
        handler->filtered_y_q8 = (int32_t)y * TOUCH_FILTER_Q8_ONE;
        handler->filter_seeded = true;
    }
    else
    {
        elapsed_ms = timestamp_ms - handler->last_timestamp_ms;
        distance = (uint32_t)touch_abs_diff(x, handler->last_x) +
                   touch_abs_diff(y, handler->last_y);
        speed = elapsed_ms == 0U ? UINT32_MAX
              : distance * 1000U / elapsed_ms;
        alpha = speed >= handler->config.fast_move_threshold_px_per_s
              ? handler->config.fast_filter_alpha_q8
              : handler->config.slow_filter_alpha_q8;
        handler->filtered_x_q8 +=
            alpha * ((int32_t)x * TOUCH_FILTER_Q8_ONE -
                     handler->filtered_x_q8) / TOUCH_FILTER_Q8_ONE;
        handler->filtered_y_q8 +=
            alpha * ((int32_t)y * TOUCH_FILTER_Q8_ONE -
                     handler->filtered_y_q8) / TOUCH_FILTER_Q8_ONE;
    }

    *output_x = (uint16_t)((handler->filtered_x_q8 + 128) /
                           TOUCH_FILTER_Q8_ONE);
    *output_y = (uint16_t)((handler->filtered_y_q8 + 128) /
                           TOUCH_FILTER_Q8_ONE);

    if (handler->pressed &&
        touch_abs_diff(*output_x, handler->last_x) <=
            handler->config.move_deadband_px &&
        touch_abs_diff(*output_y, handler->last_y) <=
            handler->config.move_deadband_px)
    {
        *output_x = handler->last_x;
        *output_y = handler->last_y;
        handler->filtered_x_q8 = (int32_t)*output_x * TOUCH_FILTER_Q8_ONE;
        handler->filtered_y_q8 = (int32_t)*output_y * TOUCH_FILTER_Q8_ONE;
    }
}

static touch_status_t touch_report_missing(bsp_touch_handler_t *handler,
                                           uint32_t timestamp_ms,
                                           touch_point_t *point)
{
    handler->press_count = 0U;
    handler->jump_pending = false;

    if (!handler->pressed)
    {
        *point = (touch_point_t){0};
        return TOUCH_NO_TOUCH;
    }

    handler->release_count++;
    *point = (touch_point_t){
        .x = handler->last_x,
        .y = handler->last_y,
        .event = handler->release_count >=
                 handler->config.release_debounce_samples
               ? TOUCH_EVENT_UP : TOUCH_EVENT_MOVE,
        .fingers = handler->release_count >=
                   handler->config.release_debounce_samples ? 0U : 1U,
        .timestamp_ms = timestamp_ms,
        .sequence = ++handler->sequence,
    };

    if (point->event == TOUCH_EVENT_UP)
    {
        handler->pressed = false;
        handler->release_count = 0U;
        handler->filter_seeded = false;
        handler->history_count = 0U;
        handler->history_index = 0U;
    }
    return TOUCH_OK;
}

touch_status_t touch_handler_init(
    bsp_touch_handler_t *handler, const touch_iic_interface_t *i2c,
    const touch_control_interface_t *control,
    const touch_yield_interface_t *yield)
{
    touch_status_t status;
    touch_info_t info;

    if (handler == NULL || i2c == NULL || control == NULL || yield == NULL)
        return TOUCH_ERROR_PARAMETER;

    memset(handler, 0, sizeof(*handler));
    status = bsp_touch_inst(&handler->driver, i2c, yield, control);
    if (status != TOUCH_OK) return status;

    status = handler->driver.pf_get_info(&handler->driver, &info);
    if (status != TOUCH_OK || info.width < 2U || info.height < 2U)
        return TOUCH_ERROR_RESOURCE;

    handler->config = (touch_processing_config_t){
        .raw_x_min = 0U,
        .raw_x_max = (uint16_t)(info.width - 1U),
        .raw_y_min = 0U,
        .raw_y_max = (uint16_t)(info.height - 1U),
        .output_width = info.width,
        .output_height = info.height,
        .move_deadband_px = TOUCH_DEFAULT_DEADBAND_PX,
        .fast_move_threshold_px_per_s = TOUCH_DEFAULT_FAST_SPEED_PX_S,
        .jump_threshold_px = TOUCH_DEFAULT_JUMP_PX,
        .jump_confirm_distance_px = TOUCH_DEFAULT_JUMP_CONFIRM_PX,
        .slow_filter_alpha_q8 = TOUCH_DEFAULT_SLOW_ALPHA_Q8,
        .fast_filter_alpha_q8 = TOUCH_DEFAULT_FAST_ALPHA_Q8,
        .press_debounce_samples = TOUCH_DEFAULT_PRESS_SAMPLES,
        .release_debounce_samples = TOUCH_DEFAULT_RELEASE_SAMPLES,
        .median_filter_enabled = true,
    };
    handler->initialized = true;
    return TOUCH_OK;
}

touch_status_t touch_handler_get_info(const bsp_touch_handler_t *handler,
                                      touch_info_t *info)
{
    touch_info_t driver_info;
    touch_status_t status;

    if (handler == NULL || info == NULL || !handler->initialized ||
        handler->driver.pf_get_info == NULL)
        return TOUCH_ERROR_RESOURCE;

    status = handler->driver.pf_get_info(&handler->driver, &driver_info);
    if (status != TOUCH_OK) return status;

    *info = (touch_info_t){
        .width = handler->config.output_width,
        .height = handler->config.output_height,
        .max_points = driver_info.max_points,
    };
    return TOUCH_OK;
}

touch_status_t touch_handler_set_processing_config(
    bsp_touch_handler_t *handler,
    const touch_processing_config_t *config)
{
    if (handler == NULL || !handler->initialized ||
        !touch_processing_config_is_valid(config))
        return TOUCH_ERROR_PARAMETER;

    handler->config = *config;
    touch_handler_reset_runtime(handler);
    return TOUCH_OK;
}

touch_status_t touch_handler_get_processing_config(
    const bsp_touch_handler_t *handler,
    touch_processing_config_t *config)
{
    if (handler == NULL || config == NULL || !handler->initialized)
        return TOUCH_ERROR_PARAMETER;

    *config = handler->config;
    return TOUCH_OK;
}

touch_status_t touch_handler_read(bsp_touch_handler_t *handler,
                                  uint32_t timestamp_ms,
                                  touch_point_t *point)
{
    touch_point_t raw_point;
    touch_status_t status;
    uint16_t mapped_x;
    uint16_t mapped_y;
    uint16_t median_x;
    uint16_t median_y;
    uint16_t output_x;
    uint16_t output_y;

    if (handler == NULL || !handler->initialized || point == NULL ||
        handler->driver.pf_read_point == NULL)
        return TOUCH_ERROR_RESOURCE;

    status = handler->driver.pf_read_point(&handler->driver, &raw_point);
    if (status == TOUCH_NO_TOUCH)
        return touch_report_missing(handler, timestamp_ms, point);
    if (status != TOUCH_OK) return status;

    status = touch_map_point(&handler->config, &raw_point,
                             &mapped_x, &mapped_y);
    if (status != TOUCH_OK)
        return touch_report_missing(handler, timestamp_ms, point);

    handler->release_count = 0U;
    if (!handler->pressed)
    {
        handler->press_count++;
        if (handler->press_count < handler->config.press_debounce_samples)
        {
            *point = (touch_point_t){0};
            return TOUCH_NO_TOUCH;
        }
    }

    touch_push_history(handler, mapped_x, mapped_y, &median_x, &median_y);
    if (!touch_jump_is_accepted(handler, median_x, median_y))
    {
        *point = (touch_point_t){
            .x = handler->last_x,
            .y = handler->last_y,
            .gesture = raw_point.gesture,
            .event = TOUCH_EVENT_MOVE,
            .fingers = 1U,
            .timestamp_ms = timestamp_ms,
            .sequence = ++handler->sequence,
        };
        return TOUCH_OK;
    }

    touch_apply_iir(handler, timestamp_ms, median_x, median_y,
                    &output_x, &output_y);
    *point = (touch_point_t){
        .x = output_x,
        .y = output_y,
        .gesture = raw_point.gesture,
        .event = handler->pressed ? TOUCH_EVENT_MOVE : TOUCH_EVENT_DOWN,
        .fingers = 1U,
        .timestamp_ms = timestamp_ms,
        .sequence = ++handler->sequence,
    };
    handler->pressed = true;
    handler->press_count = 0U;
    handler->last_x = output_x;
    handler->last_y = output_y;
    handler->last_timestamp_ms = timestamp_ms;
    return TOUCH_OK;
}

touch_status_t touch_handler_sleep(bsp_touch_handler_t *handler)
{
    touch_status_t status;

    if (handler == NULL || !handler->initialized ||
        handler->driver.pf_sleep == NULL)
        return TOUCH_ERROR_RESOURCE;

    status = handler->driver.pf_sleep(&handler->driver);
    if (status == TOUCH_OK)
    {
        handler->initialized = false;
        touch_handler_reset_runtime(handler);
    }
    return status;
}

touch_status_t touch_handler_wakeup(bsp_touch_handler_t *handler)
{
    touch_status_t status;

    if (handler == NULL || handler->driver.pf_wakeup == NULL)
        return TOUCH_ERROR_RESOURCE;

    status = handler->driver.pf_wakeup(&handler->driver);
    if (status == TOUCH_OK)
    {
        handler->initialized = true;
        touch_handler_reset_runtime(handler);
    }
    return status;
}
