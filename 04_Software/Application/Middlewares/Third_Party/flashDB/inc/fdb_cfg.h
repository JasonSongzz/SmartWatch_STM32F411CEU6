#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_

/* Project configuration for FlashDB on an external SPI NOR Flash. */
#define FDB_USING_KVDB
#define FDB_USING_TSDB
#define FDB_USING_FAL_MODE
#define FDB_USING_TIMESTAMP_64BIT

/* SPI NOR Flash supports bit-level (1 -> 0) programming. */
#define FDB_WRITE_GRAN 1

/* #define FDB_DEBUG_ENABLE */

#endif /* _FDB_CFG_H_ */
