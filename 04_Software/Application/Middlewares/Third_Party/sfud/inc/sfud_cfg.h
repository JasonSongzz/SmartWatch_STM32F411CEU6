#ifndef _SFUD_CFG_H_
#define _SFUD_CFG_H_

#define SFUD_DEBUG_MODE

#define SFUD_USING_SFDP

#define SFUD_USING_FLASH_INFO_TABLE

#define SFUD_USING_QSPI

enum {
    SFUD_FLASH_CODE_INDEX = 0,   /* DRV_FLASH_ROLE_CODE: QSPI Bank1 */
    SFUD_FLASH_DATA_INDEX,       /* DRV_FLASH_ROLE_DATA: SPI2 */

    SFUD_FLASH_DEVICE_NUM,
};

#define SFUD_FLASH_DEVICE_TABLE                                                \
{                                                                              \
    [SFUD_FLASH_CODE_INDEX]  = {.name = "flash_code",  .spi.name = "QSPI"},   \
    [SFUD_FLASH_DATA_INDEX]  = {.name = "flash_data",  .spi.name = "SPI2"},   \
}

#endif /* _SFUD_CFG_H_ */
