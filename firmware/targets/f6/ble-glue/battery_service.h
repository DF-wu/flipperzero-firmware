#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool battery_svc_init();

bool battery_svc_update_level(uint8_t battery_level);

#ifdef __cplusplus
}
#endif
