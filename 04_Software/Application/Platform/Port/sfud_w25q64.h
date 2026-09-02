#ifndef SFUD_W25Q64_H
#define SFUD_W25Q64_H

#include <stdbool.h>
#include "main.h"
#include "sfud.h"

/* W25Q64 CS uses PB12 by default; change these macros with the PCB mapping. */
#ifndef W25Q64_CS_PORT
#define W25Q64_CS_PORT GPIOB
#endif
#ifndef W25Q64_CS_PIN
#define W25Q64_CS_PIN GPIO_PIN_12
#endif

bool sfud_w25q64_init(void);
sfud_flash *sfud_w25q64_get(void);

#endif /* SFUD_W25Q64_H */
