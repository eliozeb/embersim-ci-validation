/* host_main.c — EmberSim CI host runner */
#include "mock_hal.h"
#include "ember_sim_kernel.h"
#include "trace_log.h"
#include <stdio.h>

extern void app_init(void);
extern void app_run(void);

int main(int argc, char **argv) {
    const char *trace_path = (argc > 1) ? argv[1] : "trace.jsonl";
    trace_log_init(trace_path);
    kernel_init();
    nvic_enable(28);
    nvic_enable(38);
    app_init();
    kernel_run_until(50000);
    trace_log_close();
    printf("Done.\n");
    return 0;
}
