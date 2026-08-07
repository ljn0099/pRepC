#include <stdbool.h>
#include <stdint.h>

#include <windows.h>

#include <bcrypt.h>

#define WINDOWS_TICK 10000000ULL
#define SEC_TO_UNIX_EPOCH 11644473600ULL

bool hal_system_time_unix_u64(uint64_t *time) {
    if (time == NULL)
        return false;

    FILETIME ft;
    ULARGE_INTEGER uli;

    GetSystemTimeAsFileTime(&ft);

    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    *time = (uli.QuadPart / WINDOWS_TICK) - SEC_TO_UNIX_EPOCH;

    return true;
}

bool hal_system_time_unix_u32(uint32_t *time) {
    uint64_t unixTime;

    if (time == NULL)
        return false;

    if (!hal_system_time_unix_u64(&unixTime))
        return false;

    *time = (uint32_t)unixTime;
    return true;
}

bool hal_system_random_u32(uint32_t *value) {
    if (value == NULL)
        return false;

    NTSTATUS status =
        BCryptGenRandom(NULL, (PUCHAR)value, sizeof(*value), BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    return BCRYPT_SUCCESS(status);
}
