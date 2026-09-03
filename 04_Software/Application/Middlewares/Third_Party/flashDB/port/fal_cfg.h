#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#include "spiflash_layout.h"

#include <stddef.h>

#define FAL_PART_HAS_TABLE_CFG 1

#define FAL_SPI_FLASH_DEV_NAME "spi_flash"

struct fal_flash_dev;
extern struct fal_flash_dev g_fal_spi_flash;

#define FAL_FLASH_DEV_TABLE                                                \
    {                                                                      \
        &g_fal_spi_flash,                                                  \
    }

#define FAL_PART_TABLE                                                     \
    {                                                                      \
        {FAL_PART_MAGIC_WORD, SPIFLASH_OTA_PARTITION_NAME,                 \
         FAL_SPI_FLASH_DEV_NAME, (long)SPIFLASH_OTA_OFFSET,                \
         (size_t)SPIFLASH_OTA_SIZE, 0},                                    \
        {FAL_PART_MAGIC_WORD, SPIFLASH_CONFIG_MAIN_PARTITION_NAME,         \
         FAL_SPI_FLASH_DEV_NAME, (long)SPIFLASH_CONFIG_MAIN_OFFSET,        \
         (size_t)SPIFLASH_CONFIG_MAIN_SIZE, 0},                            \
        {FAL_PART_MAGIC_WORD, SPIFLASH_CONFIG_BACKUP_PARTITION_NAME,       \
         FAL_SPI_FLASH_DEV_NAME, (long)SPIFLASH_CONFIG_BACKUP_OFFSET,      \
         (size_t)SPIFLASH_CONFIG_BACKUP_SIZE, 0},                          \
        {FAL_PART_MAGIC_WORD, SPIFLASH_LOG_PARTITION_NAME,                 \
         FAL_SPI_FLASH_DEV_NAME, (long)SPIFLASH_LOG_OFFSET,                \
         (size_t)SPIFLASH_LOG_SIZE, 0},                                    \
        {FAL_PART_MAGIC_WORD, SPIFLASH_IMAGE_PARTITION_NAME,               \
         FAL_SPI_FLASH_DEV_NAME, (long)SPIFLASH_IMAGE_OFFSET,              \
         (size_t)SPIFLASH_IMAGE_SIZE, 0},                                  \
    }

#endif /* _FAL_CFG_H_ */
