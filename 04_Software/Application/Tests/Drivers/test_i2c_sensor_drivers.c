#include "bsp_aht21_driver.h"
#include "bsp_cst816t_driver.h"
#include "bsp_mpu6050_driver.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint8_t received[256];
    uint8_t sent[1024];
    size_t received_size;
    size_t received_index;
    size_t sent_size;
    uint32_t init_count;
    uint32_t start_count;
    uint32_t stop_count;
    uint32_t ack_count;
    uint32_t nack_count;
    uint32_t lock_count;
    uint32_t unlock_count;
    uint32_t lock_depth;
    uint32_t max_lock_depth;
    bool fail_ack;
} test_i2c_bus_t;

static uint32_t s_yields[32];
static size_t s_yield_count;

static void test_yield(uint32_t milliseconds)
{
    assert(s_yield_count < sizeof(s_yields) / sizeof(s_yields[0]));
    s_yields[s_yield_count++] = milliseconds;
}

static void bus_reset(test_i2c_bus_t *bus)
{
    memset(bus, 0, sizeof(*bus));
    memset(s_yields, 0, sizeof(s_yields));
    s_yield_count = 0U;
}

static void bus_enqueue(test_i2c_bus_t *bus, const uint8_t *data, size_t size)
{
    assert(bus->received_size + size <= sizeof(bus->received));
    memcpy(&bus->received[bus->received_size], data, size);
    bus->received_size += size;
}

static void bus_send(test_i2c_bus_t *bus, uint8_t data)
{
    assert(bus->sent_size < sizeof(bus->sent));
    bus->sent[bus->sent_size++] = data;
}

static uint8_t bus_receive(test_i2c_bus_t *bus)
{
    assert(bus->received_index < bus->received_size);
    return bus->received[bus->received_index++];
}

static bool near_float(float lhs, float rhs)
{
    return fabsf(lhs - rhs) < 0.01f;
}

static uint8_t aht_crc8(const uint8_t *data, size_t size)
{
    uint8_t crc = 0xFFU;
    size_t index;
    uint8_t bit;

    for (index = 0U; index < size; ++index)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit)
            crc = (crc & 0x80U) != 0U
                ? (uint8_t)((crc << 1U) ^ 0x31U)
                : (uint8_t)(crc << 1U);
    }
    return crc;
}

#define DEFINE_I2C_ADAPTER(prefix, status_type, ok_value, parameter_error,     \
                           resource_error, timeout_error)                      \
    static status_type prefix##_init(void *context)                            \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        ++bus->init_count;                                                      \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_deinit(void *context)                          \
    {                                                                          \
        return context != NULL ? ok_value : parameter_error;                   \
    }                                                                          \
    static status_type prefix##_start(void *context)                           \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        ++bus->start_count;                                                     \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_stop(void *context)                            \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        ++bus->stop_count;                                                      \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_wait_ack(void *context)                        \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        if (bus->fail_ack) { bus->fail_ack = false; return timeout_error; }    \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_ack(void *context)                             \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        ++bus->ack_count;                                                       \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_nack(void *context)                            \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        ++bus->nack_count;                                                      \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_send(void *context, uint8_t data)              \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL) return parameter_error;                               \
        bus_send(bus, data);                                                    \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_receive(void *context, uint8_t *data)          \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL || data == NULL) return parameter_error;               \
        *data = bus_receive(bus);                                               \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_lock(void *context, uint32_t timeout_ms)       \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        (void)timeout_ms;                                                       \
        if (bus == NULL) return parameter_error;                               \
        if (bus->lock_depth != 0U) return resource_error;                      \
        ++bus->lock_depth;                                                      \
        ++bus->lock_count;                                                      \
        if (bus->lock_depth > bus->max_lock_depth)                             \
            bus->max_lock_depth = bus->lock_depth;                             \
        return ok_value;                                                       \
    }                                                                          \
    static status_type prefix##_unlock(void *context)                          \
    {                                                                          \
        test_i2c_bus_t *bus = context;                                         \
        if (bus == NULL || bus->lock_depth == 0U) return resource_error;       \
        --bus->lock_depth;                                                      \
        ++bus->unlock_count;                                                    \
        return ok_value;                                                       \
    }

DEFINE_I2C_ADAPTER(aht, temp_humi_status_t, TEMP_HUMI_OK,
                   TEMP_HUMI_ERROR_PARAMETER, TEMP_HUMI_ERROR_RESOURCE,
                   TEMP_HUMI_ERROR_TIMEOUT)
DEFINE_I2C_ADAPTER(mpu, accel_status_t, ACCEL_OK, ACCEL_ERROR_PARAMETER,
                   ACCEL_ERROR_RESOURCE, ACCEL_ERROR_TIMEOUT)
DEFINE_I2C_ADAPTER(cst, touch_status_t, TOUCH_OK, TOUCH_ERROR_PARAMETER,
                   TOUCH_ERROR_RESOURCE, TOUCH_ERROR_TIMEOUT)

static temp_humi_iic_interface_t aht_interface(test_i2c_bus_t *bus)
{
    return (temp_humi_iic_interface_t){
        .bus_context = bus, .pf_iic_init = aht_init,
        .pf_iic_deinit = aht_deinit, .pf_iic_start = aht_start,
        .pf_iic_stop = aht_stop, .pf_iic_wait_ack = aht_wait_ack,
        .pf_iic_send_ack = aht_ack, .pf_iic_send_no_ack = aht_nack,
        .pf_iic_send_byte = aht_send, .pf_iic_receive_byte = aht_receive,
        .pf_lock = aht_lock, .pf_unlock = aht_unlock,
    };
}

static accel_iic_interface_t mpu_interface(test_i2c_bus_t *bus)
{
    return (accel_iic_interface_t){
        .bus_context = bus, .pf_iic_init = mpu_init,
        .pf_iic_deinit = mpu_deinit, .pf_iic_start = mpu_start,
        .pf_iic_stop = mpu_stop, .pf_iic_wait_ack = mpu_wait_ack,
        .pf_iic_send_ack = mpu_ack, .pf_iic_send_no_ack = mpu_nack,
        .pf_iic_send_byte = mpu_send, .pf_iic_receive_byte = mpu_receive,
        .pf_lock = mpu_lock, .pf_unlock = mpu_unlock,
    };
}

static touch_iic_interface_t cst_interface(test_i2c_bus_t *bus)
{
    return (touch_iic_interface_t){
        .bus_context = bus, .pf_iic_init = cst_init,
        .pf_iic_deinit = cst_deinit, .pf_iic_start = cst_start,
        .pf_iic_stop = cst_stop, .pf_iic_wait_ack = cst_wait_ack,
        .pf_iic_send_ack = cst_ack, .pf_iic_send_no_ack = cst_nack,
        .pf_iic_send_byte = cst_send, .pf_iic_receive_byte = cst_receive,
        .pf_lock = cst_lock, .pf_unlock = cst_unlock,
    };
}

static void assert_bus_balanced(const test_i2c_bus_t *bus)
{
    assert(bus->lock_depth == 0U);
    assert(bus->lock_count == bus->unlock_count);
    assert(bus->max_lock_depth == 1U);
    assert(bus->stop_count == bus->lock_count);
}

static void test_aht21_protocol(void)
{
    test_i2c_bus_t bus;
    temp_humi_iic_interface_t iic;
    temp_humi_yield_interface_t yield = {.pf_rtos_yield = test_yield};
    bsp_temp_humi_driver_t driver = {0};
    uint8_t initial_status = AHT21_STATUS_CALIBRATED;
    uint8_t frame[7] = {0x08U, 0x80U, 0x00U, 0x06U, 0x00U, 0x00U, 0U};
    float temperature;
    float humidity;

    bus_reset(&bus);
    iic = aht_interface(&bus);
    bus_enqueue(&bus, &initial_status, 1U);
    assert(aht21_inst(&driver, &iic, &yield) == TEMP_HUMI_OK);
    assert(driver.is_inited && driver.i2c_address == AHT21_REG_I2C_ADDRESS);
    assert(s_yield_count == 1U && s_yields[0] == AHT21_POWER_ON_WAIT_MS);

    frame[6] = aht_crc8(frame, 6U);
    bus_enqueue(&bus, frame, sizeof(frame));
    assert(driver.pf_read_temp_humi(&driver, &temperature, &humidity) ==
           TEMP_HUMI_OK);
    assert(near_float(temperature, 25.0f));
    assert(near_float(humidity, 50.0f));
    assert(s_yields[s_yield_count - 1U] == AHT21_MEASURE_WAIT_MS);

    frame[6] ^= 0x01U;
    bus_enqueue(&bus, frame, sizeof(frame));
    assert(driver.pf_read_temp_humi(&driver, &temperature, &humidity) ==
           TEMP_HUMI_ERROR);
    frame[0] = AHT21_STATUS_BUSY;
    frame[6] = aht_crc8(frame, 6U);
    bus_enqueue(&bus, frame, sizeof(frame));
    assert(driver.pf_read_temp_humi(&driver, &temperature, &humidity) ==
           TEMP_HUMI_ERROR_TIMEOUT);
    assert_bus_balanced(&bus);
}

static void put_i16_be(uint8_t *data, int16_t value)
{
    data[0] = (uint8_t)((uint16_t)value >> 8U);
    data[1] = (uint8_t)value;
}

static void test_mpu6050_protocol(void)
{
    test_i2c_bus_t bus;
    accel_iic_interface_t iic;
    accel_yield_interface_t yield = {.pf_rtos_yield = test_yield};
    bsp_accel_driver_t driver = {0};
    accel_imu_data_t imu;
    uint8_t id = MPU6050_I2C_ADDRESS;
    uint8_t frame[MPU6050_IMU_FRAME_LENGTH] = {0};

    bus_reset(&bus);
    iic = mpu_interface(&bus);
    bus_enqueue(&bus, &id, 1U);
    assert(mpu6050_inst(&driver, &iic, &yield) == ACCEL_OK);
    assert(driver.is_inited);
    assert(near_float(driver.accel_sensitivity,
                      MPU6050_ACCEL_SENSITIVITY_4G));
    assert(near_float(driver.gyro_sensitivity,
                      MPU6050_GYRO_SENSITIVITY_500DPS));

    put_i16_be(&frame[0], 8192);
    put_i16_be(&frame[2], -8192);
    put_i16_be(&frame[4], 4096);
    put_i16_be(&frame[6], 340);
    put_i16_be(&frame[8], 655);
    put_i16_be(&frame[10], -655);
    put_i16_be(&frame[12], 0);
    bus_enqueue(&bus, frame, sizeof(frame));
    assert(driver.pf_read_imu(&driver, &imu) == ACCEL_OK);
    assert(near_float(imu.accel_g.x, 1.0f));
    assert(near_float(imu.accel_g.y, -1.0f));
    assert(near_float(imu.accel_g.z, 0.5f));
    assert(near_float(imu.gyro_dps.x, 10.0f));
    assert(near_float(imu.gyro_dps.y, -10.0f));
    assert(near_float(imu.temperature_c, 37.53f));

    assert(driver.pf_sleep(&driver) == ACCEL_OK);
    bus_enqueue(&bus, &id, 1U);
    assert(driver.pf_wakeup(&driver) == ACCEL_OK);
    assert_bus_balanced(&bus);
}

typedef struct
{
    bool reset_high;
    uint32_t reset_edges;
} touch_control_test_t;

static void touch_set_reset(void *context, bool high)
{
    touch_control_test_t *control = context;
    control->reset_high = high;
    ++control->reset_edges;
}

static bool touch_irq_asserted(void *context)
{
    (void)context;
    return false;
}

static void test_cst816t_protocol(void)
{
    test_i2c_bus_t bus;
    touch_iic_interface_t iic;
    touch_yield_interface_t yield = {.pf_rtos_yield = test_yield};
    touch_control_test_t control_state = {0};
    touch_control_interface_t control = {
        .context = &control_state,
        .pf_set_reset = touch_set_reset,
        .pf_is_interrupt_asserted = touch_irq_asserted,
    };
    bsp_touch_driver_t driver = {0};
    touch_point_t point;
    const uint8_t identity[2] = {0xB5U, 0x12U};
    const uint8_t pressed[6] = {1U, 1U, 0x81U, 0x23U, 0x00U, 0xABU};
    const uint8_t released[6] = {0U};

    bus_reset(&bus);
    iic = cst_interface(&bus);
    bus_enqueue(&bus, identity, sizeof(identity));
    assert(cst816t_inst(&driver, &iic, &yield, &control) == TOUCH_OK);
    assert(driver.initialized && driver.chip_id == identity[0] &&
           driver.firmware_version == identity[1]);
    assert(control_state.reset_high && control_state.reset_edges == 2U);
    assert(s_yield_count == 2U && s_yields[0] == 5U &&
           s_yields[1] == 100U);

    bus_enqueue(&bus, pressed, sizeof(pressed));
    assert(driver.pf_read_point(&driver, &point) == TOUCH_OK);
    assert(point.gesture == 1U && point.fingers == 1U);
    assert(point.x == 0x123U && point.y == 0x0ABU && point.event == 2U);

    bus_enqueue(&bus, released, sizeof(released));
    assert(driver.pf_read_point(&driver, &point) == TOUCH_NO_TOUCH);
    assert(driver.pf_sleep(&driver) == TOUCH_OK && !driver.initialized);
    assert_bus_balanced(&bus);
}

static void test_i2c_error_cleanup(void)
{
    test_i2c_bus_t bus;
    temp_humi_iic_interface_t iic;
    temp_humi_yield_interface_t yield = {.pf_rtos_yield = test_yield};
    bsp_temp_humi_driver_t driver = {0};
    uint8_t status = AHT21_STATUS_CALIBRATED;

    bus_reset(&bus);
    iic = aht_interface(&bus);
    bus_enqueue(&bus, &status, 1U);
    bus.fail_ack = true;
    assert(aht21_inst(&driver, &iic, &yield) == TEMP_HUMI_ERROR_RESOURCE);
    assert_bus_balanced(&bus);
}

int main(void)
{
    test_aht21_protocol();
    test_mpu6050_protocol();
    test_cst816t_protocol();
    test_i2c_error_cleanup();
    puts("I2C sensor driver protocol tests passed");
    return 0;
}
