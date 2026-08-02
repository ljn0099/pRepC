#ifndef PSK_SYSTEM_HAL_H
#define PSK_SYSTEM_HAL_H

#include <stdint.h>
#include <stdbool.h>

bool hal_system_random_u32(uint32_t *value);

bool hal_system_time_unix_u64(uint64_t *time);
bool hal_system_time_unix_u32(uint32_t *time);

#endif
