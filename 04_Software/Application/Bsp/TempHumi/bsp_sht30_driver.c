/******************************************************************************
 * Copyright (C) 2026, Inc.(Gmbh) or its affiliates.
 * 
 * All Rights Reserved.
 * 
 * @file ec_bsp_sht30_driver.c
 * 
 * @par dependencies 
 * - ec_bsp_sht30_driver.h
 * - ec_bsp_sht30_reg.h
 * - stdio.h
 * - stdint.h
 * 
 * @author Jason Song
 * 
 * @brief Provide the HAL APIs of SHT30 and corresponding operations.
 * 
 * Processing flow:
 * 
 * 1.The sht30_inst function will instantiate the bsp_sht30_driver_t object and
 * with the needed function interface. 
 * 
 * 2.Then the users could call the IOs from instances of bsp_sht30_driver_t.
 * 
 * @version V1.0 2025-01-29
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/

//******************************** Includes *********************************//
#include "bsp_sht30_driver.h"
#include "bsp_sht30_reg.h"
#include "Debug.h"
//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define DEBUG

#define SHT30_MEASURE_WAITING_TIME  15        // Measurement time [ms] 
#define SHT30_NOT_INITED             0        // Not init. flag 
#define SHT30_INITED                 1        // Init. flag 
#define SHT30_ID_MSB              0x80        // SHT30 ID MSB (first 2 bytes of serial number)


#define CRC8_POLYNOMIAL           0x31        // CRC-8 polynomial
#define CRC8_INITIAL              0xFF        // CRC-8 initial value

//******************************** Defines **********************************//

//******************************** Variables ********************************//

/* NOTE: g_inited / g_device_id have been moved into bsp_sht30_driver_t
 *       (is_inited / device_id) to support multi-instance safely.       */

//******************************** Variables ********************************//

//******************************** Functions ********************************//

/**
 * @brief Function for calculating CRC-8 checksum.
 * 
 * Steps:
 *  1, XOR each byte of the input data with the CRC register.
 *  2, For each bit, check the most significant bit.
 *  3, Shift the CRC register left and, if the most significant bit is 1,
 *  XOR with the polynomial.
 * 
 * @param[in] p_data : Pointer to the input data.
 * @param[in] length : Length of the input data.
 * 
 * @return uint8_t : The calculated CRC-8 checksum.
 * 
 * */
static uint8_t CheckCrc8(const uint8_t *p_data, const uint8_t length)
{
    uint8_t crc = CRC8_INITIAL; // Initialize CRC register

    for (uint8_t i = 0; i < length; i++)
    {
        crc ^= p_data[i]; // XOR the current byte with the CRC

        // Process each bit of the current byte
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)
            {
                // If MSB is 1, shift and XOR with polynomial
                crc = (crc << 1) ^ CRC8_POLYNOMIAL; 
            }
            else
            {
                // Otherwise, just shift left
                crc <<= 1; 
            }
        }
    }

    return crc; // Return the final CRC value
}


/**
 * @brief Function for sending SHT30 command.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * @param[in] cmd : Command to send to SHT30.
 * 
 * @return  sht30_status_t.
 * 
 * */
static sht30_status_t __send_command(bsp_sht30_driver_t * const p_sht30_instance, uint16_t cmd)
{
#ifndef HARDWARE_IIC
    p_sht30_instance->p_iic_driver_instance->pf_critical_enter();
#endif /* End of HARDWARE_IIC */

    // Send the IIC start Signal
    p_sht30_instance->p_iic_driver_instance->pf_iic_start(p_sht30_instance->p_iic_driver_instance->bus_context);

    // Send device address for writing
    p_sht30_instance->p_iic_driver_instance->pf_iic_send_byte(p_sht30_instance->p_iic_driver_instance->bus_context, SHT30_REG_ADDR_HEATER_DIS << 1);

    // Wait the ACK of IIC slave device
    if (SHT30_OK != p_sht30_instance->p_iic_driver_instance->pf_iic_wait_ack(p_sht30_instance->p_iic_driver_instance->bus_context))
    {
        p_sht30_instance->p_iic_driver_instance->pf_iic_stop(p_sht30_instance->p_iic_driver_instance->bus_context);
#ifndef HARDWARE_IIC
        p_sht30_instance->p_iic_driver_instance->pf_critical_exit();
#endif /* End of HARDWARE_IIC */
        return SHT30_ERROR;
    }

    // Send command high byte
    p_sht30_instance->p_iic_driver_instance->pf_iic_send_byte(p_sht30_instance->p_iic_driver_instance->bus_context, (uint8_t)(cmd >> 8));

    // Wait the ACK of IIC slave device
    if (SHT30_OK != p_sht30_instance->p_iic_driver_instance->pf_iic_wait_ack(p_sht30_instance->p_iic_driver_instance->bus_context))
    {
        p_sht30_instance->p_iic_driver_instance->pf_iic_stop(p_sht30_instance->p_iic_driver_instance->bus_context);
#ifndef HARDWARE_IIC
        p_sht30_instance->p_iic_driver_instance->pf_critical_exit();
#endif /* End of HARDWARE_IIC */
        return SHT30_ERROR;
    }

    // Send command low byte
    p_sht30_instance->p_iic_driver_instance->pf_iic_send_byte(p_sht30_instance->p_iic_driver_instance->bus_context, (uint8_t)(cmd & 0xFF));

    // Wait the ACK of IIC slave device
    if (SHT30_OK != p_sht30_instance->p_iic_driver_instance->pf_iic_wait_ack(p_sht30_instance->p_iic_driver_instance->bus_context))
    {
        p_sht30_instance->p_iic_driver_instance->pf_iic_stop(p_sht30_instance->p_iic_driver_instance->bus_context);
#ifndef HARDWARE_IIC
        p_sht30_instance->p_iic_driver_instance->pf_critical_exit();
#endif /* End of HARDWARE_IIC */
        return SHT30_ERROR;
    }

    // Send the stop signal
    p_sht30_instance->p_iic_driver_instance->pf_iic_stop(p_sht30_instance->p_iic_driver_instance->bus_context);

#ifndef HARDWARE_IIC
    p_sht30_instance->p_iic_driver_instance->pf_critical_exit();
#endif /* End of HARDWARE_IIC */

    return SHT30_OK;
}


/**
 * @brief 读时序：START + 读地址 ACK 后，连续读 6 字节；前 5 字节每字节后发 ACK，最后字节后发 NACK。
 *        原实现仅在 i<4 时发 ACK，漏掉第 5 字节后的 ACK，导致第 6 字节错位、CRC 必失败。
 */
static sht30_status_t __i2c_read_6_bytes(bsp_sht30_driver_t * const p_sht30_instance,
                                        uint8_t *rx_data)
{
    uint8_t i;

#ifndef HARDWARE_IIC
    p_sht30_instance->p_iic_driver_instance->pf_critical_enter();
#endif

    p_sht30_instance->p_iic_driver_instance->pf_iic_start(p_sht30_instance->p_iic_driver_instance->bus_context);
    p_sht30_instance->p_iic_driver_instance->pf_iic_send_byte(p_sht30_instance->p_iic_driver_instance->bus_context,
        (SHT30_REG_ADDR_HEATER_DIS << 1) | 0x01);

    if (SHT30_OK != p_sht30_instance->p_iic_driver_instance->pf_iic_wait_ack(p_sht30_instance->p_iic_driver_instance->bus_context))
    {
        p_sht30_instance->p_iic_driver_instance->pf_iic_stop(p_sht30_instance->p_iic_driver_instance->bus_context);
#ifndef HARDWARE_IIC
        p_sht30_instance->p_iic_driver_instance->pf_critical_exit();
#endif
        return SHT30_ERROR;
    }

    for (i = 0; i < 5U; i++)
    {
        p_sht30_instance->p_iic_driver_instance->pf_iic_receive_byte(p_sht30_instance->p_iic_driver_instance->bus_context, &rx_data[i]);
        p_sht30_instance->p_iic_driver_instance->pf_iic_send_ack(p_sht30_instance->p_iic_driver_instance->bus_context);
    }
    p_sht30_instance->p_iic_driver_instance->pf_iic_receive_byte(p_sht30_instance->p_iic_driver_instance->bus_context, &rx_data[5]);
    p_sht30_instance->p_iic_driver_instance->pf_iic_send_no_ack(p_sht30_instance->p_iic_driver_instance->bus_context);
    p_sht30_instance->p_iic_driver_instance->pf_iic_stop(p_sht30_instance->p_iic_driver_instance->bus_context);

#ifndef HARDWARE_IIC
    p_sht30_instance->p_iic_driver_instance->pf_critical_exit();
#endif
    return SHT30_OK;
}

/**
 * @brief Function for reading SHT30 raw data.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * @param[out] raw_temp : Raw temperature data.
 * @param[out] raw_humi : Raw humidity data.
 * 
 * @return  sht30_status_t.
 * 
 * */
static sht30_status_t __read_raw_data(bsp_sht30_driver_t * const p_sht30_instance, 
                                      uint16_t *raw_temp, 
                                      uint16_t *raw_humi)
{
    uint8_t rx_data[6];

    if (SHT30_OK != __i2c_read_6_bytes(p_sht30_instance, rx_data))
    {
        return SHT30_ERROR;
    }

    uint8_t temp_crc = CheckCrc8(&rx_data[0], 2);
    uint8_t humi_crc = CheckCrc8(&rx_data[3], 2);

    if (temp_crc != rx_data[2] || humi_crc != rx_data[5])
    {
        log_e("CRC check failed! raw=[%02x %02x %02x %02x %02x %02x]",
              rx_data[0], rx_data[1], rx_data[2], rx_data[3], rx_data[4], rx_data[5]);
        return SHT30_ERROR;
    }

    *raw_temp = (uint16_t)((rx_data[0] << 8) | rx_data[1]);
    *raw_humi = (uint16_t)((rx_data[3] << 8) | rx_data[4]);

    return SHT30_OK;
}

/**
 * @brief Read serial number command 0x3780, update device_id (MSB of first word for log).
 */
static sht30_status_t sht30_read_serial_number(bsp_sht30_driver_t * const p_sht30_instance)
{
    uint8_t rx_data[6];
    sht30_status_t ret;

    ret = __send_command(p_sht30_instance, SHT30_REG_READ_SERIAL_NUMBER);
    if (ret != SHT30_OK)
    {
        log_e("Failed to send serial number command");
        return ret;
    }
    /* Datasheet: max 1 ms */
    p_sht30_instance->p_yield_instance->pf_rtos_yield(1U);

    if (SHT30_OK != __i2c_read_6_bytes(p_sht30_instance, rx_data))
    {
        log_e("Failed to read 6 bytes");
        return SHT30_ERROR;
    }

    if (CheckCrc8(&rx_data[0], 2) != rx_data[2] || CheckCrc8(&rx_data[3], 2) != rx_data[5])
    {
        log_e("SHT30 serial number CRC failed");
        return SHT30_ERROR;
    }

    /* 保存序列号高字节供诊断；完整 48bit 可按需扩展 */
    p_sht30_instance->device_id = rx_data[0];
    return SHT30_OK;
}


/**
 * @brief 返回状态；device_id 见实例成员（初始化时已读序列号）。
 */
static sht30_status_t sht30_read_id(bsp_sht30_driver_t * const p_sht30_instance)
{
    if (p_sht30_instance == NULL || p_sht30_instance->is_inited != SHT30_INITED)
    {
        return SHT30_ERRORRESOURCE;
    }
    log_d("sht30_read_id MSB=0x%02x", p_sht30_instance->device_id);
    return SHT30_OK;
}

/**
 * @brief Function for initializing SHT30 Driver Layer.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * 
 * @return  sht30_status_t.
 * 
 * */
static sht30_status_t sht30_init(bsp_sht30_driver_t * const p_sht30_instance)
{
    sht30_status_t ret = SHT30_OK;
    
    p_sht30_instance->p_yield_instance->pf_rtos_yield(300);
    
    if (NULL == p_sht30_instance->p_iic_driver_instance ||
        NULL == p_sht30_instance->p_iic_driver_instance->pf_iic_init)
    {
        log_e("p_iic_driver_instance is NULL");
        return SHT30_ERRORRESOURCE;
    }

    p_sht30_instance->p_iic_driver_instance->pf_iic_init(p_sht30_instance->p_iic_driver_instance->bus_context);

    log_d("sht30_init iic_driver_init---------");

    ret = __send_command(p_sht30_instance, SHT30_REG_SOFT_RESET);
    if (SHT30_OK != ret)
    {
        log_e("Failed to reset SHT30");
        return SHT30_ERRORRESOURCE;
    }
    /* 手册：软复位后至少等待 1ms；略加长更稳 */
    p_sht30_instance->p_yield_instance->pf_rtos_yield(15U);

    ret = __send_command(p_sht30_instance, SHT30_REG_HEATER_DISABLE);
    if (SHT30_OK != ret)
    {
        log_e("Failed to disable heater");
        return SHT30_ERRORRESOURCE;
    }

    /* 读序列号写入 device_id（此前未发 I2C，ID 恒为 0） */
    ret = sht30_read_serial_number(p_sht30_instance);
    if (ret != SHT30_OK)
    {
        log_w("SHT30 read serial failed (check ADDR pin: 0x44 vs 0x45 in bsp_sht30_reg.h)");
        p_sht30_instance->device_id = 0;
    }
    else
    {
        log_i("SHT30 serial MSB=0x%02x (expect non-zero if bus OK)", p_sht30_instance->device_id);
    }

    p_sht30_instance->is_inited = SHT30_INITED;

    return SHT30_OK;
}

/**
 * @brief Function for deinitializing SHT30 Driver Layer.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * 
 * @return  sht30_status_t.
 * 
 * */
static sht30_status_t sht30_deinit(bsp_sht30_driver_t * const p_sht30_instance)
{
    if (p_sht30_instance != NULL)
    {
        p_sht30_instance->is_inited = SHT30_NOT_INITED;
        p_sht30_instance->device_id = 0;
    }
    return SHT30_OK;
}

/**
 * @brief Function for reading SHT30 temperature and humidity
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * @param[in] temp : [float *] The pointer to reference of temp variable.
 * @param[in] humi : [float *] pointer to reference of humi variable.
 * 
 * @return  sht30_status : [uint8_t] sht30 status .
 * 
 * */
static sht30_status_t sht30_read_temp_humi(bsp_sht30_driver_t * const p_sht30_instance,
                                           float * const temp, 
                                           float * const humi)
{
    if (p_sht30_instance == NULL || p_sht30_instance->is_inited != SHT30_INITED)
    {
        return SHT30_ERRORRESOURCE;
    }
    
    uint16_t raw_temp = 0;
    uint16_t raw_humi = 0;
    sht30_status_t ret;

    // Send measurement command
    ret = __send_command(p_sht30_instance, SHT30_REG_MEASURE_POLLING_HIGH_REP);
    if(SHT30_OK != ret)
    {
        log_e("Failed to send measure command");
        return SHT30_ERROR;
    }

    // Wait for measurement to complete (max 15ms for high repeatability)
    p_sht30_instance->p_yield_instance->pf_rtos_yield(SHT30_MEASURE_WAITING_TIME);

    // Read raw data
    ret = __read_raw_data(p_sht30_instance, &raw_temp, &raw_humi);
    if(SHT30_OK != ret)
    {
        log_e("Failed to read raw data");
        return SHT30_ERROR;
    }

    // Convert raw data to temperature and humidity
    *temp = (float)raw_temp * 175.0f / 65535.0f - 45.0f;  // T = -45 + 175 * raw / 2^16
    *humi = (float)raw_humi * 100.0f / 65535.0f;         // RH = 100 * raw / 2^16

    return SHT30_OK;
}

/**
 * @brief Function for making SHT30 sleep.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * 
 * @return  sht30_status : [uint8_t] sht30 status .
 * 
 * */
static sht30_status_t sht30_sleep(bsp_sht30_driver_t * const p_sht30_instance)
{
    if (p_sht30_instance == NULL || p_sht30_instance->is_inited != SHT30_INITED)
    {
        return SHT30_ERRORRESOURCE;
    }
    return SHT30_OK;
}

/**
 * @brief Function for making SHT30 wake-up.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * 
 * @return  sht30_status : [uint8_t] sht30 status .
 * 
 * */
static sht30_status_t sht30_wakeup(bsp_sht30_driver_t * const p_sht30_instance)
{
    if (p_sht30_instance == NULL || p_sht30_instance->is_inited != SHT30_INITED)
    {
        return SHT30_ERRORRESOURCE;
    }
    return SHT30_OK;
}

/**
 * @brief Function for instantiating SHT30 object.
 * 
 * @param[in] p_sht30_instance : The pointer to object of bsp_sht30_driver_t.
 * @param[in] p_iic_driver_instance : The pointer to reference of \
                                                      iic_driver_interface_t.
 * @param[in] p_timebase_instance : The pointer to reference of \
                                                        timebase_interface_t.
 * @param[in] p_yield_instance : The pointer to reference of \
                                                           yield_interface_t.
 * 
 * @return  sht30_status : [uint8_t] sht30 status .
 * 
 * */
sht30_status_t sht30_inst(
                        bsp_sht30_driver_t * const p_sht30_instance,
                        iic_driver_interface_t * const p_iic_driver_instance,
#ifdef OS_SUPPORTING
                        yield_interface_t * const p_yield_instance,
#endif //End of OS_SUPPORTING
                        timebase_interface_t * const p_timebase_instance
                        )
 {
    log_d("sht30_inst start");
    uint8_t ret = 0;
    if(NULL == p_sht30_instance || NULL == p_iic_driver_instance)
    {
        return SHT30_ERRORPARAMETER;
    }

    /* Check if this instance is already initialized */
    if (p_sht30_instance->is_inited == SHT30_INITED)
    {
        log_w("sht30 instance already initialized");
        return SHT30_ERRORRESOURCE;
    }

    /* Initialize instance state */
    p_sht30_instance->is_inited = SHT30_NOT_INITED;
    p_sht30_instance->device_id = 0;

    p_sht30_instance->p_iic_driver_instance = p_iic_driver_instance;
    p_sht30_instance->p_timebase_instance = p_timebase_instance;
    p_sht30_instance->p_yield_instance = p_yield_instance;

    p_sht30_instance->pf_init = (sht30_status_t (*)(void * const))\
                                                        sht30_init;
    p_sht30_instance->pf_deinit = (sht30_status_t (*)(void * const))\
                                                        sht30_deinit;
    p_sht30_instance->pf_read_id = (sht30_status_t (*)(void * const))\
                                                        sht30_read_id;
    p_sht30_instance->pf_read_temp_humi = \
              (sht30_status_t (*)(void * const,float * const temp,
                                                float * const humi))\
                                                    sht30_read_temp_humi;
    p_sht30_instance->pf_sleep = (sht30_status_t (*)(void * const))\
                                                        sht30_sleep;
    p_sht30_instance->pf_wakeup = (sht30_status_t (*)(void * const))\
                                                       sht30_wakeup;


    /* call the init function */
    ret = sht30_init(p_sht30_instance);
    log_d("sht30_init ret = %d", ret);
    if(ret)
    {
        log_e("sht30 init failed");
        return SHT30_ERRORRESOURCE;

    }
    log_d("sht30_inst end");
    return SHT30_OK;
}   
 
 
//******************************** Functions ********************************//
