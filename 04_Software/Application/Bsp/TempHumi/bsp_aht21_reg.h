#ifndef __BSP_AHT21_REG_H__
#define __BSP_AHT21_REG_H__

/* AHT21 uses a fixed 7-bit I2C address. */
#define AHT21_REG_I2C_ADDRESS       0x38U

/* Commands. */
#define AHT21_REG_INITIALIZE        0xBEU
#define AHT21_REG_TRIGGER_MEASURE   0xACU
#define AHT21_REG_SOFT_RESET        0xBAU

/* Command parameters. */
#define AHT21_REG_INITIALIZE_PARAM  0x08U
#define AHT21_REG_MEASURE_PARAM_1   0x33U
#define AHT21_REG_MEASURE_PARAM_2   0x00U

/* Status register bits. */
#define AHT21_STATUS_BUSY           0x80U
#define AHT21_STATUS_CALIBRATED     0x08U

#define AHT21_MEASURE_WAIT_MS       80U
#define AHT21_INIT_WAIT_MS          10U
#define AHT21_POWER_ON_WAIT_MS      40U

#endif /* __BSP_AHT21_REG_H__ */
