#include "fal.h"

#include "sfud_w25q64.h"
#include "w25q64_layout.h"

static sfud_flash *s_fal_flash;

static int fal_w25q64_init(void);
static int fal_w25q64_read(long offset, uint8_t *buffer, size_t size);
static int fal_w25q64_write(long offset, const uint8_t *buffer, size_t size);
static int fal_w25q64_erase(long offset, size_t size);

struct fal_flash_dev g_fal_w25q64 = {
    .name = FAL_W25Q64_DEV_NAME,
    .addr = 0U,
    .len = W25Q64_TOTAL_SIZE,
    .blk_size = W25Q64_SECTOR_SIZE,
    .ops = {
        .init = fal_w25q64_init,
        .read = fal_w25q64_read,
        .write = fal_w25q64_write,
        .erase = fal_w25q64_erase,
    },
    .write_gran = 1U,
};

static bool fal_w25q64_range_valid(long offset, size_t size)
{
    if (offset < 0 || (size_t)offset > g_fal_w25q64.len) {
        return false;
    }
    return size <= (g_fal_w25q64.len - (size_t)offset);
}

static int fal_w25q64_init(void)
{
    if (!sfud_w25q64_init()) {
        return -1;
    }

    s_fal_flash = sfud_w25q64_get();
    if (s_fal_flash == NULL || !s_fal_flash->init_ok ||
        s_fal_flash->chip.capacity < W25Q64_TOTAL_SIZE) {
        s_fal_flash = NULL;
        return -1;
    }

    if (s_fal_flash->chip.erase_gran != 0U) {
        g_fal_w25q64.blk_size = s_fal_flash->chip.erase_gran;
    }
    return g_fal_w25q64.blk_size == W25Q64_SECTOR_SIZE ? 0 : -1;
}

static int fal_w25q64_read(long offset, uint8_t *buffer, size_t size)
{
    if (s_fal_flash == NULL || buffer == NULL ||
        !fal_w25q64_range_valid(offset, size)) {
        return -1;
    }
    return sfud_read(s_fal_flash, (uint32_t)offset, size, buffer) == SFUD_SUCCESS
           ? (int)size : -1;
}

static int fal_w25q64_write(long offset, const uint8_t *buffer, size_t size)
{
    if (s_fal_flash == NULL || buffer == NULL ||
        !fal_w25q64_range_valid(offset, size)) {
        return -1;
    }
    return sfud_write(s_fal_flash, (uint32_t)offset, size, buffer) == SFUD_SUCCESS
           ? (int)size : -1;
}

static int fal_w25q64_erase(long offset, size_t size)
{
    if (s_fal_flash == NULL || !fal_w25q64_range_valid(offset, size) ||
        ((uint32_t)offset % W25Q64_SECTOR_SIZE) != 0U ||
        (size % W25Q64_SECTOR_SIZE) != 0U) {
        return -1;
    }
    return sfud_erase(s_fal_flash, (uint32_t)offset, size) == SFUD_SUCCESS
           ? (int)size : -1;
}
