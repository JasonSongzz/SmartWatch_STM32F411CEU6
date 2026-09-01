#ifndef SFUD_PORT_H
#define SFUD_PORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Function signatures injected from upper layer.
 * sfud_port.c does NOT know what these do — it just calls them. */
typedef void (*sfud_port_lock_fn_t)(void *mutex);
typedef void (*sfud_port_unlock_fn_t)(void *mutex);
typedef void (*sfud_port_delay_fn_t)(void);

typedef struct {
    bool is_qspi;
    union {
        void *hspi;
        void *hqspi;
    } bus;
    void *cs_port;
    uint16_t cs_pin;

    /* Injected service callbacks + opaque mutex.
     * The upper adapter layer creates the mutex and provides these. */
    sfud_port_lock_fn_t   lock;
    sfud_port_unlock_fn_t unlock;
    sfud_port_delay_fn_t  delay;
    void *mutex;
} sfud_port_bus_t;

void sfud_port_set_bus(size_t index, const sfud_port_bus_t *bus_cfg);

#ifdef __cplusplus
}
#endif

#endif /* SFUD_PORT_H */
