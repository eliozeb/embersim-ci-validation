#ifndef EMBER_SIM_KERNEL_H
#define EMBER_SIM_KERNEL_H

#include <stdint.h>
#include <stdbool.h>

/* ---------- forward declarations ---------- */
struct EmberPeripheral;

/* ---------- generic event types ---------- */
typedef enum {
    KERN_EVT_TIM_UPDATE,
    KERN_EVT_UART_TX_DONE,
    KERN_EVT_UART_RX_DONE,
    KERN_EVT_SPI_TX_DONE,
    KERN_EVT_SPI_RX_DONE,
    KERN_EVT_I2C_MASTER_TX_DONE,
    KERN_EVT_I2C_MASTER_RX_DONE,
    KERN_EVT_DMA_DONE,
    KERN_EVT_GPIO_EDGE,
    KERN_EVT_REGISTER_CHANGED,      // payload: EmberRegChange
    KERN_EVT_COUNT
} KernelEventType;

/* ---------- register change payload ---------- */
typedef struct {
    uint32_t    base_address;
    const char *reg_name;
    uint32_t    old_value;
    uint32_t    new_value;
    const char *reason;
} EmberRegChange;

/* ---------- kernel event ---------- */
typedef struct KernelEvent {
    uint32_t        event_id;
    uint32_t        parent_id;
    uint32_t        timestamp_us;
    KernelEventType type;
    uint32_t        source;
    uint32_t        param;
    union {
        EmberRegChange reg;
    } data;
    struct KernelEvent *next;   // for internal queue
} KernelEvent;

/* ---------- peripheral operations interface ---------- */
typedef void (*PeripheralInit)(struct EmberPeripheral *p);
typedef void (*PeripheralReset)(struct EmberPeripheral *p);
typedef void (*PeripheralTick)(struct EmberPeripheral *p, uint64_t now_us);
typedef void (*PeripheralHandleEvent)(struct EmberPeripheral *p, const KernelEvent *ev);

/* ---------- peripheral interface ---------- */
typedef struct EmberPeripheral {
    const char *name;               // human‑readable, e.g. "TIM2"
    uint32_t    base_address;       // peripheral base (0x40000400, etc.)
    uint32_t    irq_number;         // NVIC IRQ number (0 if none)
    void       *state;              // opaque instance data

    /* Lifecycle methods */
    void (*init)(struct EmberPeripheral *p);
    void (*reset)(struct EmberPeripheral *p);
    void (*tick)(struct EmberPeripheral *p, uint64_t elapsed_ns);
    void (*handle_event)(struct EmberPeripheral *p, const struct KernelEvent *ev);
    void (*shutdown)(struct EmberPeripheral *p);
} EmberPeripheral;


/* ---------- Bus event types (observable notifications) ---------- */
typedef enum {
    BUS_EVT_TIMER_UPDATE,
    BUS_EVT_UART_TX_DONE,
    BUS_EVT_UART_RX_DONE,
    BUS_EVT_REGISTER_CHANGED,
    BUS_EVT_COUNT   // if you have a count, keep it last
} BusEventType;

typedef struct {
    uint32_t    base_address;
    const char *reg_name;
    uint32_t    old_value;
    uint32_t    new_value;
    const char *reason;
} BusRegChangePayload;

typedef struct BusEvent {
    uint32_t        timestamp_us;
    BusEventType    type;
    uint32_t        source;
    uint32_t        param;
    union {
        BusRegChangePayload reg;
    } data;
    struct BusEvent *next;
} BusEvent;

/* Perform one full simulation step: advance peripherals, flush hardware events,
   then dispatch any resulting interrupts. */
void kernel_step(void);

/* ---------- Bus subscriber ---------- */
typedef void (*BusSubscriberCallback)(const BusEvent *ev);

/* NVIC resolution – returns the highest priority pending IRQ, or -1 if none */
int  nvic_resolve(void);
void nvic_dispatch_irq(int irq);

typedef void (*IrqHandler)(void);
void nvic_register_handler(uint32_t irq, IrqHandler handler);

/* priority: lower number = higher priority */
void ember_bus_subscribe(int priority, BusSubscriberCallback cb);
void ember_bus_publish(BusEventType type, uint32_t source, uint32_t param,
                       const BusEvent *payload_template); // simplified later
void ember_bus_dispatch_all(void);
void ember_bus_init(void);

/* ---------- kernel API ---------- */
void     kernel_init(void);
uint64_t kernel_now_us(void);
void     kernel_register_peripheral(EmberPeripheral *p);
void     kernel_schedule_event(uint32_t delay_us, KernelEventType type,
                               uint32_t source, uint32_t param, uint32_t parent_event_id);
void     kernel_run_until(uint64_t deadline_us);

/* NVIC stubs */
void nvic_set_pending(uint32_t irq);
void nvic_clear_pending(uint32_t irq);
bool nvic_is_pending(uint32_t irq);
void nvic_enable(uint32_t irq);
void nvic_disable(uint32_t irq);

void kernel_advance_ticks(uint32_t us);
void kernel_dispatch_pending(void);


void trace_software_event(const char *component, const char *event, const char *details_json);

#endif