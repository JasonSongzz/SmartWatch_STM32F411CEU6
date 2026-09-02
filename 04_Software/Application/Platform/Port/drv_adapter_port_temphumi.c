#include "drv_adapter_port_temphumi.h"
#include "bsp_temp_humi_xxx_handler.h"
#include "iic_hal.h"
#include "osal.h"

#define SHT_CLK_ENABLE __HAL_RCC_GPIOF_CLK_ENABLE()
#define TEMP_HUMI_DATA_MUTEX_TIMEOUT_MS (100U)

typedef struct {
    bool registered;
    float temperature;
    float humidity;
    float temp_scale;
    iic_bus_t iic_bus;
    osal_mutex_handle_t data_mutex;
    iic_driver_interface_t iic_interface;
    yield_interface_t yield_interface;
    timebase_interface_t timebase_interface;
    bsp_temp_humi_sht30_handler_t handler;
} temphumi_port_t;

static temphumi_port_t s_port[TEMP_HUMI_DEV_MAX];

static sht30_status_t iic_init(void *bus) 
{ 
    SHT_CLK_ENABLE; 
    IICInit((iic_bus_t *)bus); 

    return SHT30_OK; 
}

static sht30_status_t iic_start(void *bus) 
{ 
    IICStart((iic_bus_t *)bus); 

    return SHT30_OK; 
}

static sht30_status_t iic_stop(void *bus) 
{ 
    IICStop((iic_bus_t *)bus); 

    return SHT30_OK; 
}

static sht30_status_t iic_wait_ack(void *bus) 
{ 
    return IICWaitAck((iic_bus_t *)bus) == SUCCESS ? SHT30_OK : SHT30_ERRORTIMEOUT; 
}

static sht30_status_t iic_send_ack(void *bus) 
{ 
    IICSendAck((iic_bus_t *)bus); 

    return SHT30_OK; 
}
static sht30_status_t iic_send_nack(void *bus) 
{ 
    IICSendNotAck((iic_bus_t *)bus); 

    return SHT30_OK; 
}
static sht30_status_t iic_send_byte(void *bus, const uint8_t data) 
{ 
    IICSendByte((iic_bus_t *)bus, data); 

    return SHT30_OK; 
}
static sht30_status_t iic_recv_byte(void *bus, uint8_t * const data) 
{ 
    *data = IICReceiveByte((iic_bus_t *)bus); 

    return SHT30_OK; 
}
static sht30_status_t iic_critical_enter(void) 
{ 
    osal_enter_critical(); 

    return SHT30_OK; 
}
static sht30_status_t iic_critical_exit(void) 
{ 
    osal_exit_critical(); 

    return SHT30_OK; 
}

static bool port_init(temphumi_drv_t *dev)
{
    temphumi_port_t *p = (temphumi_port_t *)dev->user_data;
    sht30_status_t ret;
    if (p == NULL) return false;

    if (p->data_mutex == NULL && osal_mutex_create(&p->data_mutex) != OSAL_SUCCESS) return false;

    p->iic_interface = (iic_driver_interface_t){
        .bus_context = &p->iic_bus,
        .pf_iic_init = iic_init,
        .pf_iic_start = iic_start,
        .pf_iic_stop = iic_stop,
        .pf_iic_wait_ack = iic_wait_ack,
        .pf_iic_send_ack = iic_send_ack,
        .pf_iic_send_no_ack = iic_send_nack,
        .pf_iic_send_byte = iic_send_byte,
        .pf_iic_receive_byte = iic_recv_byte,
        .pf_critical_enter = iic_critical_enter,
        .pf_critical_exit = iic_critical_exit,
    };
    p->yield_interface.pf_rtos_yield = osal_task_delay_ms;
    p->timebase_interface.pf_get_tick_count = HAL_GetTick;

    ret = temp_humi_handler_init(&p->handler, &p->iic_interface,
                                 &p->yield_interface, &p->timebase_interface);
    return ret == SHT30_OK;
}

static bool port_refresh(temphumi_drv_t *dev)
{
    temphumi_port_t *p = (temphumi_port_t *)dev->user_data;
    float temp, humi;

    if (p == NULL || temp_humi_handler_read(&p->handler, &temp, &humi) != SHT30_OK) return false;

    if (osal_mutex_take(p->data_mutex, TEMP_HUMI_DATA_MUTEX_TIMEOUT_MS) != OSAL_SUCCESS) return false;

    p->temperature = temp * p->temp_scale;
    p->humidity = humi;
    (void)osal_mutex_give(p->data_mutex);

    return true;
}

static void port_read_cached(temphumi_drv_t *dev, float *temp, float *humi)
{
    temphumi_port_t *p = (temphumi_port_t *)dev->user_data;
    
    if (p == NULL || temp == NULL || humi == NULL) return;

    if (p->data_mutex != NULL &&
        osal_mutex_take(p->data_mutex, TEMP_HUMI_DATA_MUTEX_TIMEOUT_MS) == OSAL_SUCCESS) {
        *temp = p->temperature; *humi = p->humidity;
        (void)osal_mutex_give(p->data_mutex);
    } else {
        *temp = p->temperature; *humi = p->humidity;
    }
}

bool drv_adapter_temphumi_register(uint32_t index, const temphumi_port_config_t *config)
{
    temphumi_drv_t drv;
    temphumi_port_t *p;

    if (index >= TEMP_HUMI_DEV_MAX || s_port[index].registered) return false;
    p = &s_port[index];

    if (config != NULL && config->iic_bus != NULL) p->iic_bus = *(iic_bus_t *)config->iic_bus;
    else p->iic_bus = (iic_bus_t){ .IIC_SDA_PORT = GPIOF, .IIC_SDA_PIN = GPIO_PIN_11,
                                   .IIC_SCL_PORT = GPIOF, .IIC_SCL_PIN = GPIO_PIN_12 };

    p->temp_scale = (config != NULL && config->temp_scale > 0.0f) ? config->temp_scale : 1.5f;
    p->temperature = 0.0f; p->humidity = 0.0f; p->data_mutex = NULL;
    drv.idx = index; drv.user_data = p; drv.init = port_init;
    drv.refresh = port_refresh; drv.read_cached = port_read_cached;

    if (!drv_adapter_temphumi_reg(index, &drv)) return false;
    p->registered = true;

    return true;
}
