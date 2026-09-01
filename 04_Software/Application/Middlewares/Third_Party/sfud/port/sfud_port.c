#include <sfud.h>
#include <sfud_port.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

#define SFUD_SPI_TIMEOUT_MS  (1000U)
#define SFUD_PORT_RETRY_TIMES 10000U

static char log_buf[256];

/* ── Per-instance port context ────────────────────────────────────────── */
typedef struct {
    bool used;
    bool is_qspi;
    union {
        SPI_HandleTypeDef *hspi;
        QSPI_HandleTypeDef *hqspi;
    } bus;
    GPIO_TypeDef *cs_port;
    uint16_t cs_pin;

    /* Injected from upper layer — opaque */
    sfud_port_lock_fn_t   lock_fn;
    sfud_port_unlock_fn_t unlock_fn;
    sfud_port_delay_fn_t  delay_fn;
    void *mutex;
} sfud_port_inst_t;

static sfud_port_inst_t s_port_inst[SFUD_FLASH_DEVICE_NUM];

void sfud_port_set_bus(size_t index, const sfud_port_bus_t *bus_cfg)
{
    sfud_port_inst_t *inst;

    if ((bus_cfg == NULL) || (index >= SFUD_FLASH_DEVICE_NUM)) {
        return;
    }

    inst = &s_port_inst[index];
    if (inst->used) {
        return;
    }

    (void)memset(inst, 0, sizeof(*inst));
    inst->used     = true;
    inst->is_qspi  = bus_cfg->is_qspi;
    inst->cs_port  = (GPIO_TypeDef *)bus_cfg->cs_port;
    inst->cs_pin   = bus_cfg->cs_pin;
    inst->lock_fn  = bus_cfg->lock;
    inst->unlock_fn = bus_cfg->unlock;
    inst->delay_fn = bus_cfg->delay;
    inst->mutex    = bus_cfg->mutex;

    if (bus_cfg->is_qspi) {
        inst->bus.hqspi = (QSPI_HandleTypeDef *)bus_cfg->bus.hqspi;
    } else {
        inst->bus.hspi = (SPI_HandleTypeDef *)bus_cfg->bus.hspi;
    }
}

/* ── CS helpers (SPI mode only) ─────────────────────────────────────── */
static void sfud_cs_low(const sfud_port_inst_t *inst)
{
    if ((inst != NULL) && (inst->cs_port != NULL)) {
        HAL_GPIO_WritePin(inst->cs_port, inst->cs_pin, GPIO_PIN_RESET);
    }
}

static void sfud_cs_high(const sfud_port_inst_t *inst)
{
    if ((inst != NULL) && (inst->cs_port != NULL)) {
        HAL_GPIO_WritePin(inst->cs_port, inst->cs_pin, GPIO_PIN_SET);
    }
}

/* ── Lock / unlock (delegated to injected callbacks) ────────────────── */
static void spi_lock(const sfud_spi *spi)
{
    sfud_port_inst_t *inst;

    if (spi == NULL) {
        return;
    }
    inst = (sfud_port_inst_t *)spi->user_data;
    if ((inst != NULL) && (inst->lock_fn != NULL) && (inst->mutex != NULL)) {
        inst->lock_fn(inst->mutex);
    }
}

static void spi_unlock(const sfud_spi *spi)
{
    sfud_port_inst_t *inst;

    if (spi == NULL) {
        return;
    }
    inst = (sfud_port_inst_t *)spi->user_data;
    if ((inst != NULL) && (inst->unlock_fn != NULL) && (inst->mutex != NULL)) {
        inst->unlock_fn(inst->mutex);
    }
}

/* ── SPI mode wr callback (pure HAL, no OSAL) ───────────────────────── */
static sfud_err spi_write_read(const sfud_spi *spi,
                               const uint8_t *write_buf, size_t write_size,
                               uint8_t *read_buf, size_t read_size)
{
    sfud_port_inst_t *inst;
    sfud_err result = SFUD_SUCCESS;

    if ((spi == NULL) || (spi->user_data == NULL)) {
        return SFUD_ERR_NOT_FOUND;
    }
    inst = (sfud_port_inst_t *)spi->user_data;

    sfud_cs_low(inst);

    if (write_size > 0U) {
        if (HAL_SPI_Transmit(inst->bus.hspi, (uint8_t *)write_buf,
                             (uint16_t)write_size, SFUD_SPI_TIMEOUT_MS) != HAL_OK) {
            result = SFUD_ERR_TIMEOUT;
        }
    }
    if ((result == SFUD_SUCCESS) && (read_size > 0U)) {
        if (read_buf == NULL) {
            result = SFUD_ERR_READ;
        } else if (HAL_SPI_Receive(inst->bus.hspi, read_buf,
                                   (uint16_t)read_size, SFUD_SPI_TIMEOUT_MS) != HAL_OK) {
            result = SFUD_ERR_TIMEOUT;
        }
    }

    sfud_cs_high(inst);
    return result;
}

/* ── QSPI mode wr callback (pure HAL, no OSAL) ──────────────────────── */
static sfud_err qspi_write_read(const sfud_spi *spi,
                                const uint8_t *write_buf, size_t write_size,
                                uint8_t *read_buf, size_t read_size)
{
    sfud_port_inst_t *inst;
    QSPI_HandleTypeDef *hqspi;
    QSPI_CommandTypeDef cmd;
    size_t data_offset;
    uint8_t addr_bytes;
    HAL_StatusTypeDef hal_ret;

    if ((spi == NULL) || (spi->user_data == NULL)) {
        return SFUD_ERR_NOT_FOUND;
    }
    inst = (sfud_port_inst_t *)spi->user_data;
    hqspi = inst->bus.hqspi;

    if ((write_size == 0U) || (write_buf == NULL)) {
        return SFUD_ERR_WRITE;
    }

    (void)memset(&cmd, 0, sizeof(cmd));
    cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction       = write_buf[0];
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles       = 0U;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    if (write_size == 1U) {
        addr_bytes = 0U;
        data_offset = 1U;
    } else if (write_size == 4U) {
        addr_bytes = 3U;
        data_offset = 4U;
    } else if (write_size == 5U) {
        addr_bytes = 4U;
        data_offset = 5U;
    } else {
        addr_bytes = 3U;
        data_offset = 4U;
    }

    if (addr_bytes > 0U) {
        cmd.AddressMode = QSPI_ADDRESS_1_LINE;
        cmd.AddressSize = (addr_bytes == 4U) ? QSPI_ADDRESS_32_BITS : QSPI_ADDRESS_24_BITS;

        if (addr_bytes == 4U) {
            cmd.Address = ((uint32_t)write_buf[1] << 24) |
                          ((uint32_t)write_buf[2] << 16) |
                          ((uint32_t)write_buf[3] << 8)  |
                          ((uint32_t)write_buf[4]);
        } else {
            cmd.Address = ((uint32_t)write_buf[1] << 16) |
                          ((uint32_t)write_buf[2] << 8)  |
                          ((uint32_t)write_buf[3]);
        }
    }

    if ((write_size > data_offset) || (read_size > 0U)) {
        cmd.DataMode = QSPI_DATA_1_LINE;
        cmd.NbData   = (write_size > data_offset) ? (uint32_t)(write_size - data_offset)
                                                  : (uint32_t)read_size;
    }

    hal_ret = HAL_QSPI_Command(hqspi, &cmd, SFUD_SPI_TIMEOUT_MS);
    if (hal_ret != HAL_OK) {
        return SFUD_ERR_TIMEOUT;
    }

    if (write_size > data_offset) {
        hal_ret = HAL_QSPI_Transmit(hqspi, (uint8_t *)&write_buf[data_offset],
                                    SFUD_SPI_TIMEOUT_MS);
        if (hal_ret != HAL_OK) {
            return SFUD_ERR_TIMEOUT;
        }
    }

    if ((read_size > 0U) && (read_buf != NULL)) {
        hal_ret = HAL_QSPI_Receive(hqspi, read_buf, SFUD_SPI_TIMEOUT_MS);
        if (hal_ret != HAL_OK) {
            return SFUD_ERR_TIMEOUT;
        }
    }

    return SFUD_SUCCESS;
}

#ifdef SFUD_USING_QSPI

static uint32_t sfud_map_inst_lines(uint8_t lines)
{
    switch (lines) {
    case 1:  return QSPI_INSTRUCTION_1_LINE;
    case 2:  return QSPI_INSTRUCTION_2_LINES;
    case 4:  return QSPI_INSTRUCTION_4_LINES;
    default: return QSPI_INSTRUCTION_NONE;
    }
}

static uint32_t sfud_map_addr_lines(uint8_t lines)
{
    switch (lines) {
    case 1:  return QSPI_ADDRESS_1_LINE;
    case 2:  return QSPI_ADDRESS_2_LINES;
    case 4:  return QSPI_ADDRESS_4_LINES;
    default: return QSPI_ADDRESS_NONE;
    }
}

static uint32_t sfud_map_data_lines(uint8_t lines)
{
    switch (lines) {
    case 1:  return QSPI_DATA_1_LINE;
    case 2:  return QSPI_DATA_2_LINES;
    case 4:  return QSPI_DATA_4_LINES;
    default: return QSPI_DATA_NONE;
    }
}

static sfud_err qspi_read(const struct __sfud_spi *spi, uint32_t addr,
                          sfud_qspi_read_cmd_format *qspi_read_cmd_format,
                          uint8_t *read_buf, size_t read_size)
{
    sfud_port_inst_t *inst;
    QSPI_HandleTypeDef *hqspi;
    QSPI_CommandTypeDef cmd;
    HAL_StatusTypeDef hal_ret;

    if ((spi == NULL) || (spi->user_data == NULL) || (qspi_read_cmd_format == NULL)) {
        return SFUD_ERR_NOT_FOUND;
    }
    if ((read_buf == NULL) || (read_size == 0U)) {
        return SFUD_ERR_READ;
    }

    inst = (sfud_port_inst_t *)spi->user_data;
    hqspi = inst->bus.hqspi;

    (void)memset(&cmd, 0, sizeof(cmd));
    cmd.InstructionMode   = sfud_map_inst_lines(qspi_read_cmd_format->instruction_lines);
    cmd.Instruction       = qspi_read_cmd_format->instruction;
    cmd.AddressMode       = sfud_map_addr_lines(qspi_read_cmd_format->address_lines);
    cmd.AddressSize       = (qspi_read_cmd_format->address_size == 32U)
                            ? QSPI_ADDRESS_32_BITS : QSPI_ADDRESS_24_BITS;
    cmd.Address           = addr;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode          = sfud_map_data_lines(qspi_read_cmd_format->data_lines);
    cmd.DummyCycles       = qspi_read_cmd_format->dummy_cycles;
    cmd.NbData            = (uint32_t)read_size;
    cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

    hal_ret = HAL_QSPI_Command(hqspi, &cmd, SFUD_SPI_TIMEOUT_MS);
    if (hal_ret != HAL_OK) {
        return SFUD_ERR_TIMEOUT;
    }

    hal_ret = HAL_QSPI_Receive(hqspi, read_buf, SFUD_SPI_TIMEOUT_MS);
    if (hal_ret != HAL_OK) {
        return SFUD_ERR_TIMEOUT;
    }

    return SFUD_SUCCESS;
}
#endif

/* ── sfud_spi_port_init (called by SFUD per device) ─────────────────── */
sfud_err sfud_spi_port_init(sfud_flash *flash)
{
    sfud_port_inst_t *inst;

    if ((flash == NULL) || (flash->index >= SFUD_FLASH_DEVICE_NUM)) {
        return SFUD_ERR_NOT_FOUND;
    }

    inst = &s_port_inst[flash->index];
    if (!inst->used) {
        return SFUD_ERR_NOT_FOUND;
    }

    flash->spi.wr        = inst->is_qspi ? qspi_write_read : spi_write_read;
    flash->spi.lock      = spi_lock;
    flash->spi.unlock    = spi_unlock;
    flash->spi.user_data = inst;

#ifdef SFUD_USING_QSPI
    flash->spi.qspi_read = qspi_read;
#endif

    flash->retry.delay  = (void (*)(void))inst->delay_fn;
    flash->retry.times  = SFUD_PORT_RETRY_TIMES;

    return SFUD_SUCCESS;
}

/* ── Log output ─────────────────────────────────────────────────────── */
void sfud_log_debug(const char *file, const long line, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    (void)snprintf(log_buf, sizeof(log_buf), "[SFUD](%s:%ld) ", file, line);
    printf("%s", log_buf);
    (void)vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\n", log_buf);
    va_end(args);
}

void sfud_log_info(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    printf("[SFUD]");
    (void)vsnprintf(log_buf, sizeof(log_buf), format, args);
    printf("%s\n", log_buf);
    va_end(args);
}
