/*
 * FlashDB configuration — TSDB + FAL + SFUD (W25Q64).
 */
#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_

#define FDB_USING_KVDB
#define FDB_USING_TSDB
#define FDB_USING_FAL_MODE

#define FDB_WRITE_GRAN 1

/* Log to RTT (same viewer as EasyLogger) */
#include "SEGGER_RTT.h"
#define FDB_PRINT(...) SEGGER_RTT_printf(0, __VA_ARGS__)

/* #define FDB_DEBUG_ENABLE */

#endif /* _FDB_CFG_H_ */
