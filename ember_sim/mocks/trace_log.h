#ifndef TRACE_LOG_H
#define TRACE_LOG_H

#include <stdint.h>

void trace_log_init(const char *filename);
void trace_log_close(void);

/* json_payload: valid JSON key-value fragment, e.g. "\"inst\":\"0x40011000\",\"sz\":5" */
void trace_log(const char *func, const char *json_payload);

#endif