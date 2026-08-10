#ifndef PREPC_SYSTEM_HAL_H
#define PREPC_SYSTEM_HAL_H

#include <stdbool.h>
#include <stdint.h>

bool prepc_system_random_u32(uint32_t *value);

bool prepc_system_time_unix_u64(uint64_t *time);
bool prepc_system_time_unix_u32(uint32_t *time);

#endif
