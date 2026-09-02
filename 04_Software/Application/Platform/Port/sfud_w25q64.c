#include "sfud_w25q64.h"

static bool s_w25q64_initialized;

bool sfud_w25q64_init(void)
{
    if (s_w25q64_initialized) return true;

    if (sfud_init() != SFUD_SUCCESS)
    {
        return false;
    }
    s_w25q64_initialized = true;
    
    return true;
}

sfud_flash *sfud_w25q64_get(void)
{
    return s_w25q64_initialized ? sfud_get_device(SFUD_FLASH_DATA_INDEX) : NULL;
}
