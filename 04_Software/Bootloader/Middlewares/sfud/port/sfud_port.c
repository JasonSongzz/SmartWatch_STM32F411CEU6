/*
 * SFUD port: SPI1 (PA5/6/7) + PA4 software CS, STM32 HAL + OSAL mutex/delay.
 */
#include <sfud.h>
#include <stdarg.h>
#include <stdio.h>

#include "main.h"
#include "spi.h"
#include "osal.h"
#include "osal_error.h"
#include "stm32f4xx_hal.h"

#ifndef W25Q64_CS_GPIO_Port
#define W25Q64_CS_GPIO_Port GPIOA
#endif
#ifndef W25Q64_CS_Pin
#define W25Q64_CS_Pin GPIO_PIN_4
#endif

#define SFUD_SPI_TIMEOUT_MS (1000U)

static char log_buf[256];

static osal_mutex_handle_t s_sfud_spi_mutex;
static uint8_t s_sfud_mutex_inited;

void sfud_log_debug(const char *file, const long line, const char *format, ...);

static void sfud_cs_low(void)
{
    HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_RESET);
}

static void sfud_cs_high(void)
{
    HAL_GPIO_WritePin(W25Q64_CS_GPIO_Port, W25Q64_CS_Pin, GPIO_PIN_SET);
}

static void sfud_retry_delay(void)
{
    osal_task_delay_ms(1U);
}

static void spi_lock(const sfud_spi *spi)
{
    (void)spi;
    if (s_sfud_mutex_inited != 0U) {
        (void)osal_mutex_take(s_sfud_spi_mutex, OSAL_MAX_DELAY);
    }
}

static void spi_unlock(const sfud_spi *spi)
{
    (void)spi;
    if (s_sfud_mutex_inited != 0U) {
        (void)osal_mutex_give(s_sfud_spi_mutex);
    }
}

static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf, size_t write_size, uint8_t *read_buf,
                               size_t read_size)
{
    sfud_err result = SFUD_SUCCESS;
    SPI_HandleTypeDef *hspi;

    if (spi == NULL || spi->user_data == NULL) {
        return SFUD_ERR_NOT_FOUND;
    }
    hspi = (SPI_HandleTypeDef *)spi->user_data;

    sfud_cs_low();

    if (write_size > 0U) {
        if (HAL_SPI_Transmit(hspi, (uint8_t *)write_buf, (uint16_t)write_size, SFUD_SPI_TIMEOUT_MS) != HAL_OK) {
            result = SFUD_ERR_TIMEOUT;
        }
    }
    if (result == SFUD_SUCCESS && read_size > 0U) {
        if (read_buf == NULL) {
            result = SFUD_ERR_READ;
        } else if (HAL_SPI_Receive(hspi, read_buf, (uint16_t)read_size, SFUD_SPI_TIMEOUT_MS) != HAL_OK) {
            result = SFUD_ERR_TIMEOUT;
        }
    }

    sfud_cs_high();

    return result;
}

#ifdef SFUD_USING_QSPI
static sfud_err qspi_read(const struct __sfud_spi *spi, uint32_t addr, sfud_qspi_read_cmd_format *qspi_read_cmd_format,
                          uint8_t *read_buf, size_t read_size)
{
    (void)spi;
    (void)addr;
    (void)qspi_read_cmd_format;
    (void)read_buf;
    (void)read_size;
    return SFUD_ERR_NOT_FOUND;
}
#endif /* SFUD_USING_QSPI */

sfud_err sfud_spi_port_init(sfud_flash *flash)
{
    if (flash == NULL) {
        return SFUD_ERR_NOT_FOUND;
    }

    if (s_sfud_mutex_inited == 0U) {
        if (osal_mutex_create(&s_sfud_spi_mutex) == OSAL_SUCCESS) {
            s_sfud_mutex_inited = 1U;
        }
    }

    flash->spi.wr = spi_write_read;
    flash->spi.lock = spi_lock;
    flash->spi.unlock = spi_unlock;
    flash->spi.user_data = &hspi1;
#ifdef SFUD_USING_QSPI
    flash->spi.qspi_read = qspi_read;
#endif
    flash->retry.times = 10000U;
    flash->retry.delay = sfud_retry_delay;

    return SFUD_SUCCESS;
}

void sfud_log_debug(const char *file, const long line, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    printf("[SFUD](%s:%ld) ", file, line);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\n", log_buf);
    va_end(args);
}

void sfud_log_info(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    printf("[SFUD]");
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\n", log_buf);
    va_end(args);
}
