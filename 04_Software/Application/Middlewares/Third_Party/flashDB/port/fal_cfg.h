/*
 * FAL: one W25Q64 device + elog TSDB partition 0x300000–0x5FFFFF (3 MiB).
 */
#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#include <fal_def.h>
#include <stddef.h>

#define FAL_PART_HAS_TABLE_CFG 1
#define FAL_PART_MAGIC_WORD 0x45503130

#define FAL_W25Q64_DEV_NAME "w25q64"

extern struct fal_flash_dev g_fal_w25q64;

#define FAL_FLASH_DEV_TABLE                                                                                            \
    {                                                                                                                  \
        &g_fal_w25q64,                                                                                                 \
    }

/* Partition offset on W25Q64 (absolute); length = 0x5FFFFF - 0x300000 + 1 */
#define FAL_PART_LOG_TSDB_OFFSET ((long)0x00300000U)
#define FAL_PART_LOG_TSDB_LEN ((size_t)0x00300000U)

#define FAL_PART_TABLE                                                                                                 \
    {                                                                                                                  \
        {FAL_PART_MAGIC_WORD, "log_tsdb", FAL_W25Q64_DEV_NAME, FAL_PART_LOG_TSDB_OFFSET, FAL_PART_LOG_TSDB_LEN, 0},    \
    }

#endif /* _FAL_CFG_H_ */
