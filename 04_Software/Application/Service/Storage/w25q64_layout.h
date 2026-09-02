#ifndef W25Q64_LAYOUT_H
#define W25Q64_LAYOUT_H

#include <stdint.h>

/* W25Q64 capacity: 8 MiB, sector size: 4 KiB. All regions are sector-aligned. */
#define W25Q64_TOTAL_SIZE          0x00800000UL
#define W25Q64_SECTOR_SIZE         0x00001000UL

/* 512 KiB reserved for an OTA image backup. */
#define W25Q64_OTA_OFFSET          0x00000000UL
#define W25Q64_OTA_SIZE            0x00080000UL

/* Two independent 64 KiB configuration slots. */
#define W25Q64_CONFIG_MAIN_OFFSET  0x00080000UL
#define W25Q64_CONFIG_MAIN_SIZE    0x00010000UL
#define W25Q64_CONFIG_BACKUP_OFFSET 0x00090000UL
#define W25Q64_CONFIG_BACKUP_SIZE  0x00010000UL

/* Reserved for future persistent data. */
#define W25Q64_RESERVED_OFFSET     0x000A0000UL
#define W25Q64_RESERVED_SIZE       0x00060000UL

/* 1 MiB append-only log area. */
#define W25Q64_LOG_OFFSET          0x00100000UL
#define W25Q64_LOG_SIZE            0x00100000UL

/* Remaining 6 MiB for LVGL images and other read-only resources. */
#define W25Q64_IMAGE_OFFSET        0x00200000UL
#define W25Q64_IMAGE_SIZE          0x00600000UL

#if ((W25Q64_CONFIG_MAIN_OFFSET % W25Q64_SECTOR_SIZE) != 0U) || \
    ((W25Q64_CONFIG_BACKUP_OFFSET % W25Q64_SECTOR_SIZE) != 0U) || \
    ((W25Q64_LOG_OFFSET % W25Q64_SECTOR_SIZE) != 0U) || \
    ((W25Q64_IMAGE_OFFSET % W25Q64_SECTOR_SIZE) != 0U)
#error "W25Q64 partition offsets must be sector-aligned"
#endif

#if ((W25Q64_CONFIG_MAIN_OFFSET + W25Q64_CONFIG_MAIN_SIZE) != W25Q64_CONFIG_BACKUP_OFFSET) || \
    ((W25Q64_CONFIG_BACKUP_OFFSET + W25Q64_CONFIG_BACKUP_SIZE) != W25Q64_RESERVED_OFFSET) || \
    ((W25Q64_RESERVED_OFFSET + W25Q64_RESERVED_SIZE) != W25Q64_LOG_OFFSET) || \
    ((W25Q64_LOG_OFFSET + W25Q64_LOG_SIZE) != W25Q64_IMAGE_OFFSET)
#error "W25Q64 partition layout contains an unexpected gap or overlap"
#endif

#if ((W25Q64_IMAGE_OFFSET + W25Q64_IMAGE_SIZE) != W25Q64_TOTAL_SIZE)
#error "W25Q64 partition layout must consume exactly the device capacity"
#endif

#endif /* W25Q64_LAYOUT_H */
