#include <stdbool.h>
#include <stdint.h>
#include <sys/random.h>
#include <time.h>

bool prepc_system_time_unix_u64(uint64_t *time) {
    if (!time)
        return false;

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return false;

    *time = (uint64_t)ts.tv_sec;
    return true;
}

bool prepc_system_time_unix_u32(uint32_t *time) {
    uint64_t unixTime;

    if (time == NULL)
        return false;

    if (!prepc_system_time_unix_u64(&unixTime))
        return false;

    *time = (uint32_t)unixTime;
    return true;
}

bool prepc_system_random_u32(uint32_t *value) {
    ssize_t read;

    if (value == NULL)
        return false;

    read = getrandom(value, sizeof(*value), 0);
    if (read != (ssize_t)sizeof(*value))
        return false;

    return true;
}
