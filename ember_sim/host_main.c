// host_main.c
#include "mock_hal.h"
#include "trace_log.h"
#include "ember_sim_kernel.h"

extern void app_init(void);
extern void app_run(void);

int main(int argc, char **argv) {
    const char *trace_path = (argc > 1) ? argv[1] : "trace.jsonl";
    trace_log_init(trace_path);
    kernel_init();
    app_init();

    uint64_t deadline = 1000000;  // 1 second
    while (kernel_now_us() < deadline) {
        kernel_step();
        app_run();                // user firmware loop
    }

    trace_log_close();
    return 0;
}