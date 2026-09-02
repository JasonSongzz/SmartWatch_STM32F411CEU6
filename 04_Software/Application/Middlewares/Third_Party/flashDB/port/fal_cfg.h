/* W25Q64: OTA image, redundant configuration, logs and LVGL resources. */
#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#include <fal_def.h>
#include <stddef.h>
#include "w25q64_layout.h"

#define FAL_PART_HAS_TABLE_CFG 1
#define FAL_PART_MAGIC_WORD 0x45503130

#define FAL_W25Q64_DEV_NAME "w25q64"

extern struct fal_flash_dev g_fal_w25q64;

#define FAL_FLASH_DEV_TABLE                                                                                            \
    {                                                                                                                  \
        &g_fal_w25q64,                                                                                                 \
    }

#define FAL_PART_OTA_OFFSET           ((long)W25Q64_OTA_OFFSET)
#define FAL_PART_OTA_LEN              ((size_t)W25Q64_OTA_SIZE)
#define FAL_PART_CONFIG_MAIN_OFFSET   ((long)W25Q64_CONFIG_MAIN_OFFSET)
#define FAL_PART_CONFIG_MAIN_LEN      ((size_t)W25Q64_CONFIG_MAIN_SIZE)
#define FAL_PART_CONFIG_BACKUP_OFFSET ((long)W25Q64_CONFIG_BACKUP_OFFSET)
#define FAL_PART_CONFIG_BACKUP_LEN    ((size_t)W25Q64_CONFIG_BACKUP_SIZE)
#define FAL_PART_LOG_TSDB_OFFSET      ((long)W25Q64_LOG_OFFSET)
#define FAL_PART_LOG_TSDB_LEN         ((size_t)W25Q64_LOG_SIZE)
#define FAL_PART_IMAGE_OFFSET         ((long)W25Q64_IMAGE_OFFSET)
#define FAL_PART_IMAGE_LEN            ((size_t)W25Q64_IMAGE_SIZE)

#define FAL_PART_TABLE                                                                                                 \
    {                                                                                                                  \
        {FAL_PART_MAGIC_WORD, "ota_backup", FAL_W25Q64_DEV_NAME, FAL_PART_OTA_OFFSET, FAL_PART_OTA_LEN, 0},             \
        {FAL_PART_MAGIC_WORD, "config_main", FAL_W25Q64_DEV_NAME, FAL_PART_CONFIG_MAIN_OFFSET, FAL_PART_CONFIG_MAIN_LEN, 0}, \
        {FAL_PART_MAGIC_WORD, "config_backup", FAL_W25Q64_DEV_NAME, FAL_PART_CONFIG_BACKUP_OFFSET, FAL_PART_CONFIG_BACKUP_LEN, 0}, \
        {FAL_PART_MAGIC_WORD, "log_tsdb", FAL_W25Q64_DEV_NAME, FAL_PART_LOG_TSDB_OFFSET, FAL_PART_LOG_TSDB_LEN, 0},    \
        {FAL_PART_MAGIC_WORD, "image", FAL_W25Q64_DEV_NAME, FAL_PART_IMAGE_OFFSET, FAL_PART_IMAGE_LEN, 0},             \
    }

#endif /* _FAL_CFG_H_ */
