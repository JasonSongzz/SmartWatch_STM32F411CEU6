#ifndef _SFUD_CFG_H_
#define _SFUD_CFG_H_

#define SFUD_DEBUG_MODE

#define SFUD_USING_SFDP

#define SFUD_USING_FLASH_INFO_TABLE

enum {
    SFUD_FLASH_DATA_INDEX = 0,   /* W25Q64: SPI2 */

    SFUD_FLASH_DEVICE_NUM,
};

#define SFUD_FLASH_DEVICE_TABLE                                                \
{                                                                              \
    [SFUD_FLASH_DATA_INDEX]  = {.name = "w25q64", .spi.name = "SPI2"},       \
}

#endif /* _SFUD_CFG_H_ */
