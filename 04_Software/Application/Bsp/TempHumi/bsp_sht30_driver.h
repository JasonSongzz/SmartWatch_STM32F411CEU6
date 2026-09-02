/******************************************************************************
 * Copyright (C) 2026, Inc.(Gmbh) or its affiliates.
 * 
 * All Rights Reserved.
 * 
 * @file ec_bsp_sht30_driver.h
 * 
 * @par dependencies 
 * - ec_bsp_sht30_reg.h
 * - stdio.h
 * - stdint.h
 * 
 * @author Jason Song   
 * 
 * @brief Provide the HAL APIs of SHT30 and corresponding operations.
 * 
 * Processing flow:
 * 
 * call directly.
 * 
 * @version V1.0 2025-01-29
 *
 * @note 1 tab == 4 spaces!
 * 
 *****************************************************************************/

#ifndef __BSP_SHT30_DRIVER_H__
#define __BSP_SHT30_DRIVER_H__


//******************************** Includes *********************************//

#include "bsp_sht30_reg.h"

#include <stdio.h>
#include <stdint.h>

//******************************** Includes *********************************//

//******************************** Defines **********************************//
#define OS_SUPPORTING
//#define HARDWARE_IIC

/*  函数返回状态枚举                    */
typedef enum
{
  SHT30_OK                = 0,         /* Operation completed successfully.  */
  SHT30_ERROR             = 1,         /* Run-time error without case matched*/
  SHT30_ERRORTIMEOUT      = 2,         /* Operation failed with timeout      */
  SHT30_ERRORRESOURCE     = 3,         /* Resource not available.            */
  SHT30_ERRORPARAMETER    = 4,         /* Parameter error.                   */
  SHT30_ERRORNOMEMORY     = 5,         /* Out of memory.                     */
  SHT30_ERRORISR          = 6,         /* Not allowed in ISR context         */
  SHT30_RESERVED          = 0x7FFFFFFF /* Reserved                           */
} sht30_status_t;

//******************************** Defines **********************************//

//******************************** Declaring ********************************//

/* From Core Layer：  IIC Port        */
#ifndef HARDWARE_IIC     /* True : Software IIC */
typedef struct 
{
    void *bus_context;                               /* 实例上下文，指向 iic_bus_t */
    sht30_status_t (*pf_iic_init)          (void*);  /*   IIC init    interf.*/
    sht30_status_t (*pf_iic_deinit)        (void*);  /*   IIC deinit  interf.*/
    sht30_status_t (*pf_iic_start)         (void*);  /*   IIC start   interf.*/
    sht30_status_t (*pf_iic_stop)          (void*);  /*   IIC stop    interf.*/
    sht30_status_t (*pf_iic_wait_ack)      (void*);  /*   IIC w-ack   interf.*/
    sht30_status_t (*pf_iic_send_ack)      (void*);  /*   IIC s-ack   interf.*/
    sht30_status_t (*pf_iic_send_no_ack)   (void*);  /*   IIC s-n-ack interf.*/
    sht30_status_t (*pf_iic_send_byte)     (void*,   /*   IIC s-byte  interf.*/
                                            const    uint8_t);
    sht30_status_t (*pf_iic_receive_byte)  (void*,   /*   IIC r-byte  interf.*/
                                            uint8_t * const );  
    sht30_status_t (*pf_critical_enter)     (void);  /* enter critical state.*/
    sht30_status_t (*pf_critical_exit )     (void);  /* exit  critical state.*/
} iic_driver_interface_t;
#endif /* End of HARDWARE_IIC */

#ifdef HARDWARE_IIC     /* True : Hardware IIC */
typedef struct
{
    sht30_status_t (*pfiic_init)           (void *); /*   IIC init    interf.*/
    sht30_status_t (*pfiic_deinit)         (void *); /*   IIC deinit  interf.*/
    sht30_status_t (*pfiic_send_ack)       (void *); /*   IIC s-ack   interf.*/
    sht30_status_t (*pfiic_send_no_ack)    (void *); /*   IIC s-n-ack interf.*/
    sht30_status_t (*pfiic_send_byte)(void *,        /*   IIC s-byte  interf.*/
                                           uint8_t);
    sht30_status_t (*pfiic_receive_byte)   (void *); /*   IIC r-byte  interf.*/
} iic_driver_interface_t;
#endif //End of HARDWARE_IIC

/* From Core Layer：  TimeBase        */
typedef struct
{
    uint32_t (*pf_get_tick_count)(void);            /*Get Tick number interf.*/
} timebase_interface_t;

/* From OS   Layer：  OS_Delay        */
#ifdef OS_SUPPORTING
typedef struct
{
    void (*pf_rtos_yield)(const uint32_t);          /*OS Not-Blocking Delay  */
}yield_interface_t;
#endif //End of OS_SUPPORTING

/* Sht30_hal_driver instance class    */
typedef struct
{
    iic_driver_interface_t *p_iic_driver_instance;
    timebase_interface_t     *p_timebase_instance;
    yield_interface_t           *p_yield_instance;

    /* Instance-private state (multi-instance safe, replaces global variables) */
    int8_t   is_inited;              /* Per-instance init flag (0=not inited) */
    uint8_t  device_id;              /* Per-instance device ID                */

    sht30_status_t (*pf_inst)(
                                       void * const      psht30_instance,
                     iic_driver_interface_t * const piic_driver_instance,
                     timebase_interface_t   * const   ptimebase_instance,
                          yield_interface_t * const      pyield_instance);
         
    sht30_status_t (*pf_init)         (void * const);/*  SHT30 init function */
    sht30_status_t (*pf_deinit)       (void * const);/*SHT30 deinit function */
    sht30_status_t (*pf_read_id)      (void * const);/*SHT30 read_id function*/
    sht30_status_t (*pf_read_temp_humi)(void * const,/*Read Temp.Humi   func.*/
                                  float * const temp,
                                 float * const humi);
    sht30_status_t (*pf_sleep)        (void * const);/*SHT30  Sleep  function*/
    sht30_status_t (*pf_wakeup)       (void * const);/*SHT30 Wake-up function*/
} bsp_sht30_driver_t;

/* Sht30_hal_driver instance class Inst. Function        */
sht30_status_t sht30_inst(                                          
                        bsp_sht30_driver_t *          const psht30_instance,
                        iic_driver_interface_t * const piic_driver_instance,
#ifdef OS_SUPPORTING
                        yield_interface_t *           const pyield_instance,
#endif //End of OS_SUPPORTING
                        timebase_interface_t *     const ptimebase_instance); 
#endif
