#ifndef __BSP_SHT30_REG_H__
#define __BSP_SHT30_REG_H__

// SHT30 I2C Address
#define SHT30_REG_ADDR_HEATER_DIS 0x44  // ADDR pin LOW
#define SHT30_REG_ADDR_HEATER_EN  0x45  // ADDR pin HIGH

// Commands (hex) for SHT30
#define SHT30_REG_MEASURE_CLOCK_STRETCHING_EN    0x2C06  // Clock stretching enabled, high repeatability
#define SHT30_REG_MEASURE_CLOCK_STRETCHING_DIS   0x2C0D  // Clock stretching disabled, high repeatability
#define SHT30_REG_MEASURE_POLLING_HIGH_REP       0x2400  // Polling, high repeatability
#define SHT30_REG_MEASURE_POLLING_MED_REP        0x240B  // Polling, medium repeatability
#define SHT30_REG_MEASURE_POLLING_LOW_REP        0x2416  // Polling, low repeatability
#define SHT30_REG_MEASURE_PERIODE_HIGH_REP       0x2130  // Periodic, high repeatability
#define SHT30_REG_MEASURE_PERIODE_MED_REP        0x2236  // Periodic, medium repeatability
#define SHT30_REG_MEASURE_PERIODE_LOW_REP        0x2334  // Periodic, low repeatability
#define SHT30_REG_FETCH_DATA                     0xE000  // Fetch data from alert
#define SHT30_REG_ART                            0x2B32  // ART (Accelerated Response Time)
#define SHT30_REG_HEATER_ENABLE                  0x306D  // Heater enable
#define SHT30_REG_HEATER_DISABLE                 0x3066  // Heater disable
#define SHT30_REG_SOFT_RESET                     0x30A2  // Soft reset
#define SHT30_REG_ALERT_CLEAR                    0x3041  // Clear All Alert Flags
#define SHT30_REG_STATUS_REG_CLEAR               0x3045  // Clear Status Register
#define SHT30_REG_READ_STATUS_REG                0xF32D  // Read Status Register
#define SHT30_REG_READ_SERIAL_NUMBER             0x3780  // Read Serial Number

#endif /* __BSP_SHT30_REG_H__ */
