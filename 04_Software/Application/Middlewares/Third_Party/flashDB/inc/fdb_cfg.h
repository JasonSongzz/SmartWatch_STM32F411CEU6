/*
 * FlashDB configuration — TSDB + FAL + SFUD (W25Q64).
 */
#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_

#define FDB_USING_KVDB
#define FDB_USING_TSDB
#define FDB_USING_FAL_MODE
#define FDB_USING_TIMESTAMP_64BIT

#define FDB_WRITE_GRAN 1

/* #define FDB_DEBUG_ENABLE */

#endif /* _FDB_CFG_H_ */
