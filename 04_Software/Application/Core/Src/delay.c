#include "delay.h"

#include "cmsis_os.h"
#include "stm32f4xx_hal.h"

static uint8_t s_dwt_initialized;

void delay_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    s_dwt_initialized = 1U;
}

void delay_us(uint32_t us)
{
    uint32_t start;
    uint32_t cycles;

    if (s_dwt_initialized == 0U)
    {
        delay_init();
    }

    cycles = (uint32_t)(((uint64_t)SystemCoreClock * us) / 1000000ULL);
    start = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - start) < cycles)
    {
    }
}

void delay_ms(uint32_t ms)
{
    if (__get_IPSR() != 0U)
    {
        while (ms-- > 0U)
        {
            delay_us(1000U);
        }
    }
    else if (osKernelGetState() == osKernelRunning)
    {
        (void)osDelay(ms);
    }
    else
    {
        HAL_Delay(ms);
    }
}
