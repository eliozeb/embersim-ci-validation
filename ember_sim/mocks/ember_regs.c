#include "ember_regs.h"
#include "ember_sim_kernel.h"
#include "trace_log.h"
#include <string.h>
#include <stdio.h>

/* Forward declaration */
static void publish_reg_change(const char *peripheral, uint32_t base,
                               const char *reg_name, uint32_t old_val,
                               uint32_t new_val, const char *reason);

void ember_regs_init(EmberRegMap *map, const char *peripheral, uint32_t base,
                     EmberRegister *regs, uint32_t count) {
    map->peripheral = peripheral;
    map->base_address = base;
    map->regs = regs;
    map->count = count;
    for (uint32_t i = 0; i < count; i++) {
        regs[i].value = regs[i].reset_value;
    }
}

uint32_t ember_reg_read(EmberRegMap *map, uint32_t index) {
    return map->regs[index].value;
}

void ember_reg_write(EmberRegMap *map, uint32_t index, uint32_t value,
                     const char *reason, uint32_t parent_event_id) {
    EmberRegister *reg = &map->regs[index];
    value = (reg->value & ~reg->writable_mask) | (value & reg->writable_mask);
    uint32_t old = reg->value;
    if (old == value) return;
    reg->value = value;

    /* Publish register change event via the bus */
    publish_reg_change(map->peripheral, map->base_address, reg->name, old, reg->value, reason);

    if (reg->on_write) {
        reg->on_write(map->base_address, reg, old, reason);
    }
}

void ember_reg_set_bits(EmberRegMap *map, uint32_t index, uint32_t mask,
                        const char *reason, uint32_t parent_event_id) {
    uint32_t new_val = map->regs[index].value | mask;
    ember_reg_write(map, index, new_val, reason, parent_event_id);
}

void ember_reg_clear_bits(EmberRegMap *map, uint32_t index, uint32_t mask,
                          const char *reason, uint32_t parent_event_id) {
    uint32_t new_val = map->regs[index].value & ~mask;
    ember_reg_write(map, index, new_val, reason, parent_event_id);
}

/* Static helper – publishes a BUS_EVT_REGISTER_CHANGED */
static void publish_reg_change(const char *peripheral, uint32_t base,
                               const char *reg_name, uint32_t old_val,
                               uint32_t new_val, const char *reason) {
    BusEvent tmpl;
    memset(&tmpl, 0, sizeof(tmpl));
    tmpl.data.reg.base_address = base;
    tmpl.data.reg.reg_name = reg_name;
    tmpl.data.reg.old_value = old_val;
    tmpl.data.reg.new_value = new_val;
    tmpl.data.reg.reason = reason;
    ember_bus_publish(BUS_EVT_REGISTER_CHANGED, base, 0, &tmpl);
}