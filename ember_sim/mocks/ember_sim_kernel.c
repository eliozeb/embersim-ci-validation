#include "ember_sim_kernel.h"
#include "trace_log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_PERIPHERALS     8
#define MAX_EVENTS          64
#define MAX_BUS_SUBSCRIBERS 8
#define MAX_BUS_EVENTS      64
#define MAX_IRQ_HANDLERS    64

/* ---------- Peripheral list ---------- */
static EmberPeripheral *peripherals[MAX_PERIPHERALS];
static int              peripheral_count = 0;

/* ---------- Kernel event queue ---------- */
static KernelEvent      event_pool[MAX_EVENTS];
static KernelEvent*     free_list = NULL;
static KernelEvent*     queue_head = NULL;
static uint32_t         next_event_id = 1;
static uint64_t         sim_time_us = 0;

/* ---------- Bus event queue ---------- */
static BusEvent         bus_event_pool[MAX_BUS_EVENTS];
static BusEvent*        bus_free_list = NULL;
static BusEvent*        bus_queue_head = NULL;

/* ---------- Bus subscribers ---------- */
static struct {
    int                    priority;
    BusSubscriberCallback  callback;
} bus_subscribers[MAX_BUS_SUBSCRIBERS];
static int bus_sub_count = 0;

/* ---------- NVIC state ---------- */
static bool nvic_pending[64];
static bool nvic_enabled_flags[64];

/* ---------- IRQ handler table (generic) ---------- */
typedef void (*IrqHandler)(void);
static IrqHandler irq_handlers[MAX_IRQ_HANDLERS];

/* ---------- Forward declarations ---------- */
static void trace_bus_handler(const BusEvent *ev);
static void nvic_bus_handler(const BusEvent *ev);

/* =================================================================
   Event pool management (kernel events)
   ================================================================= */
static void init_event_pool(void) {
    for (int i = 0; i < MAX_EVENTS; i++) {
        event_pool[i].next = (i < MAX_EVENTS-1) ? &event_pool[i+1] : NULL;
    }
    free_list = &event_pool[0];
}

static KernelEvent* alloc_event(void) {
    if (!free_list) return NULL;
    KernelEvent *ev = free_list;
    free_list = ev->next;
    memset(ev, 0, sizeof(KernelEvent));
    return ev;
}

static void free_event(KernelEvent *ev) {
    ev->next = free_list;
    free_list = ev;
}

/* =================================================================
   Bus event pool management
   ================================================================= */
static void init_bus_pool(void) {
    for (int i = 0; i < MAX_BUS_EVENTS; i++) {
        bus_event_pool[i].next = (i < MAX_BUS_EVENTS-1) ? &bus_event_pool[i+1] : NULL;
    }
    bus_free_list = &bus_event_pool[0];
}

static BusEvent* bus_alloc_event(void) {
    if (!bus_free_list) return NULL;
    BusEvent *ev = bus_free_list;
    bus_free_list = ev->next;
    memset(ev, 0, sizeof(BusEvent));
    return ev;
}

static void bus_free_event(BusEvent *ev) {
    ev->next = bus_free_list;
    bus_free_list = ev;
}

/* =================================================================
   Sorted insert (kernel events)
   ================================================================= */
static void insert_event(KernelEvent *ev) {
    if (!queue_head || ev->timestamp_us < queue_head->timestamp_us) {
        ev->next = queue_head;
        queue_head = ev;
        return;
    }
    KernelEvent *cur = queue_head;
    while (cur->next && cur->next->timestamp_us <= ev->timestamp_us)
        cur = cur->next;
    ev->next = cur->next;
    cur->next = ev;
}

/* =================================================================
   Process kernel events (hardware → peripheral handling)
   ================================================================= */
static void process_events(void) {
    while (queue_head && queue_head->timestamp_us <= sim_time_us) {
        KernelEvent *ev = queue_head;
        queue_head = ev->next;

        for (int i = 0; i < peripheral_count; i++) {
            if (peripherals[i]->base_address == ev->source && peripherals[i]->handle_event) {
                peripherals[i]->handle_event(peripherals[i], ev);
            }
        }

        free_event(ev);
    }
}

/* =================================================================
   Public kernel API
   ================================================================= */
void kernel_init(void) {
    init_event_pool();
    init_bus_pool();
    peripheral_count = 0;
    next_event_id = 1;
    sim_time_us = 0;
    queue_head = NULL;
    bus_queue_head = NULL;
    bus_sub_count = 0;
    memset(nvic_pending, 0, sizeof(nvic_pending));
    memset(nvic_enabled_flags, 0, sizeof(nvic_enabled_flags));
    memset(irq_handlers, 0, sizeof(irq_handlers));

    /* built‑in subscribers */
    ember_bus_subscribe(10, nvic_bus_handler);   // NVIC reacts early
    ember_bus_subscribe(20, trace_bus_handler);  // trace after NVIC
}

uint64_t kernel_now_us(void) {
    return sim_time_us;
}

void kernel_register_peripheral(EmberPeripheral *p) {
    if (peripheral_count < MAX_PERIPHERALS) {
        peripherals[peripheral_count++] = p;
        if (p->init) p->init(p);
    }
}

void kernel_schedule_event(uint32_t delay_us, KernelEventType type,
                           uint32_t source, uint32_t param, uint32_t parent_event_id) {
    KernelEvent *ev = alloc_event();
    if (!ev) return;
    ev->event_id     = next_event_id++;
    ev->parent_id    = parent_event_id;
    ev->timestamp_us = sim_time_us + delay_us;
    ev->type         = type;
    ev->source       = source;
    ev->param        = param;
    insert_event(ev);
}

void kernel_advance_ticks(uint32_t us) {
    uint64_t ns = us * 1000ULL;
    for (int i = 0; i < peripheral_count; i++) {
        if (peripherals[i]->tick) {
            peripherals[i]->tick(peripherals[i], ns);
        }
    }
    sim_time_us += us;
}

void kernel_dispatch_pending(void) {
    ember_bus_dispatch_all();   // hardware notifications (NVIC sets pending)
    process_events();           // residual peripheral events

    /* NVIC arbitration + HAL dispatch */
    int irq;
    while ((irq = nvic_resolve()) >= 0) {
        nvic_dispatch_irq(irq);
        nvic_clear_pending(irq);
    }
}

void kernel_run_until(uint64_t deadline_us) {
    while (sim_time_us < deadline_us) {
        kernel_advance_ticks(1);
        kernel_dispatch_pending();
    }
}

void kernel_step(void) {
    kernel_advance_ticks(1);
    kernel_dispatch_pending();
}

/* =================================================================
   Event bus implementation
   ================================================================= */
void ember_bus_init(void) { /* done in kernel_init */ }

void ember_bus_subscribe(int priority, BusSubscriberCallback cb) {
    if (bus_sub_count >= MAX_BUS_SUBSCRIBERS) return;
    int i;
    for (i = 0; i < bus_sub_count; i++) {
        if (priority < bus_subscribers[i].priority) break;
    }
    for (int j = bus_sub_count; j > i; j--) {
        bus_subscribers[j] = bus_subscribers[j-1];
    }
    bus_subscribers[i].priority = priority;
    bus_subscribers[i].callback = cb;
    bus_sub_count++;
}

void ember_bus_publish(BusEventType type, uint32_t source, uint32_t param,
                       const BusEvent *payload_template) {
    BusEvent *ev = bus_alloc_event();
    if (!ev) return;
    ev->timestamp_us = sim_time_us;
    ev->type = type;
    ev->source = source;
    ev->param = param;
    if (payload_template) {
        memcpy(&ev->data, &payload_template->data, sizeof(ev->data));
    }
    ev->next = NULL;
    if (!bus_queue_head) {
        bus_queue_head = ev;
    } else {
        BusEvent *cur = bus_queue_head;
        while (cur->next) cur = cur->next;
        cur->next = ev;
    }
}

void ember_bus_dispatch_all(void) {
    while (bus_queue_head) {
        BusEvent *ev = bus_queue_head;
        bus_queue_head = ev->next;
        for (int i = 0; i < bus_sub_count; i++) {
            bus_subscribers[i].callback(ev);
        }
        bus_free_event(ev);
    }
}

/* =================================================================
   NVIC helpers
   ================================================================= */
static uint32_t periph_to_irq(uint32_t base) {
    for (int i = 0; i < peripheral_count; i++) {
        if (peripherals[i]->base_address == base && peripherals[i]->irq_number) {
            return peripherals[i]->irq_number;
        }
    }
    return 0;
}

static void nvic_bus_handler(const BusEvent *ev) {
    uint32_t irq = periph_to_irq(ev->source);
    if (irq) nvic_set_pending(irq);
}

void nvic_register_handler(uint32_t irq, IrqHandler handler) {
    if (irq < MAX_IRQ_HANDLERS) irq_handlers[irq] = handler;
}

int nvic_resolve(void) {
    for (int i = 0; i < 64; i++) {
        if (nvic_pending[i] && nvic_enabled_flags[i]) return i;
    }
    return -1;
}

void nvic_dispatch_irq(int irq) {
    char details[64];
    snprintf(details, sizeof(details), "{\"irq\":%d}", irq);
    trace_log("NVIC_DISPATCH", details);
    if (irq_handlers[irq]) {
        irq_handlers[irq]();
    }
}

void nvic_set_pending(uint32_t irq) { nvic_pending[irq] = true; }
void nvic_clear_pending(uint32_t irq) { nvic_pending[irq] = false; }
bool nvic_is_pending(uint32_t irq) { return nvic_pending[irq]; }
void nvic_enable(uint32_t irq) { nvic_enabled_flags[irq] = true; }
void nvic_disable(uint32_t irq) { nvic_enabled_flags[irq] = false; }

/* =================================================================
   Trace bus handler (subscribes to all bus events)
   ================================================================= */
static void trace_bus_handler(const BusEvent *ev) {
    switch (ev->type) {
        case BUS_EVT_TIMER_UPDATE: {
            char details[128];
            snprintf(details, sizeof(details),
                     "{\"layer\":\"hardware\",\"timer\":\"%08x\",\"cause\":\"update\"}", ev->source);
            trace_log("HARDWARE_EVENT", details);
            break;
        }
        case BUS_EVT_REGISTER_CHANGED: {
            char payload[256];
            snprintf(payload, sizeof(payload),
                     "\"origin\":\"register\",\"layer\":\"hardware\",\"peripheral\":\"%s\",\"address\":\"0x%08X\",\"register\":\"%s\",\"old\":\"0x%04X\",\"new\":\"0x%04X\",\"reason\":\"%s\"",
                     "TIM2",
                     ev->data.reg.base_address,
                     ev->data.reg.reg_name,
                     ev->data.reg.old_value,
                     ev->data.reg.new_value,
                     ev->data.reg.reason);
            trace_log("REGISTER_EVENT", payload);
            break;
        }
        case BUS_EVT_UART_TX_DONE: {
            char details[128];
            snprintf(details, sizeof(details),
                     "{\"layer\":\"hardware\",\"uart\":\"%08x\",\"event\":\"tx_done\"}", ev->source);
            trace_log("HARDWARE_EVENT", details);
            break;
        }
        case BUS_EVT_UART_RX_DONE: {
            char details[128];
            snprintf(details, sizeof(details),
                     "{\"layer\":\"hardware\",\"uart\":\"%08x\",\"event\":\"rx_done\"}", ev->source);
            trace_log("HARDWARE_EVENT", details);
            break;
        }
        default: break;
    }
}

/* =================================================================
   Software trace helper (used by mock_tim.c)
   ================================================================= */
void trace_software_event(const char *component, const char *event, const char *details_json) {
    char payload[512];
    snprintf(payload, sizeof(payload),
             "\"origin\":\"software\",\"layer\":\"hal\",\"component\":\"%s\",\"event\":\"%s\",\"details\":%s",
             component, event, details_json ? details_json : "{}");
    trace_log("SOFTWARE_EVENT", payload);
}