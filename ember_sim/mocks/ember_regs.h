#ifndef EMBER_REGS_H
#define EMBER_REGS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct EmberRegister {
    const char *name;
    uint32_t    value;
    uint32_t    reset_value;
    uint32_t    writable_mask;
    uint32_t    readonly_mask;
    void (*on_write)(uint32_t base_address, const struct EmberRegister *reg, uint32_t old_val, const char *reason);
} EmberRegister;

typedef struct {
    const char    *peripheral;
    uint32_t       base_address;
    EmberRegister *regs;
    uint32_t       count;
} EmberRegMap;

void     ember_regs_init(EmberRegMap *map, const char *peripheral, uint32_t base,
                         EmberRegister *regs, uint32_t count);
uint32_t ember_reg_read(EmberRegMap *map, uint32_t index);
void     ember_reg_write(EmberRegMap *map, uint32_t index, uint32_t value,
                         const char *reason, uint32_t parent_event_id);
void     ember_reg_set_bits(EmberRegMap *map, uint32_t index, uint32_t mask,
                            const char *reason, uint32_t parent_event_id);
void     ember_reg_clear_bits(EmberRegMap *map, uint32_t index, uint32_t mask,
                              const char *reason, uint32_t parent_event_id);

#endif