#include "trace_log.h"
#include <stdio.h>
#include <string.h>

static FILE *trace_file = NULL;
static uint32_t event_counter = 0;

void trace_log_init(const char *filename)
{
    trace_file = fopen(filename, "w");
    if (trace_file) {
        fprintf(trace_file, "# EmberSim trace v1.0\n");
    }
}

void trace_log_close(void)
{
    if (trace_file) {
        fclose(trace_file);
        trace_file = NULL;
    }
}

void trace_log(const char *func, const char *json_payload)
{
    if (!trace_file) return;

    const char *peripheral = "UNKNOWN";
    if (strncmp(func, "HAL_GPIO", 8) == 0) peripheral = "GPIO";
    else if (strncmp(func, "HAL_I2C", 7) == 0) peripheral = "I2C";
    else if (strncmp(func, "HAL_SPI", 7) == 0) peripheral = "SPI";
    else if (strncmp(func, "HAL_TIM", 7) == 0) peripheral = "TIM";
    else if (strncmp(func, "HAL_DMA", 7) == 0) peripheral = "DMA";
    else if (strncmp(func, "HAL_ADC", 7) == 0) peripheral = "ADC";
    

    if (strncmp(func, "HARDWARE_EVENT", 14) == 0) peripheral = "runtime";
    else if (strncmp(func, "SOFTWARE_EVENT", 14) == 0) peripheral = "runtime";
    else if (strncmp(func, "REGISTER_EVENT", 14) == 0) peripheral = "runtime";
    else if (strncmp(func, "NVIC_DISPATCH", 13) == 0) peripheral = "runtime";
    else if (strncmp(func, "HAL_UART", 8) == 0) peripheral = "UART";

    fprintf(trace_file,
        "{\"ts_ms\":%u,\"peripheral\":\"%s\",\"func\":\"%s\",%s}\n",
        event_counter++, peripheral, func, json_payload);

    fflush(trace_file);
}