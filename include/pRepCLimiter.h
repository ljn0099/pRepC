#ifndef PREPC_LIMITER_H
#define PREPC_LIMITER_H

#include "pRepC.h"
#include <stddef.h>
#include <stdint.h>

#define PREPC_RATE_CALLSIGN_MAX_LEN 15
#define PREPC_RATE_MODE_MAX_LEN 15

#define PREPC_RATE_MIN_INTERVAL_S (5 * 60)
#define PREPC_RATE_UNCHANGED_INTERVAL_S (60 * 60)

#define PREPC_RATE_PURGE_INTERVAL_S (12 * 60 * 60)
#define PREPC_RATE_MAX_AGE_S (24 * 60 * 60)

#define PREPC_RATE_UNKNOWN_FREQ_CHANGE_HZ 50000

typedef struct prepcRateCtx_t prepcRateCtx_t;

prepcError_t prepc_rate_ctx_init(prepcRateCtx_t **ctx, size_t maxEntries);

void prepc_rate_free(prepcRateCtx_t *ctx);

prepcError_t prepc_rate_should_report(prepcRateCtx_t *ctx, const char *callsign, size_t callsignLen,
                                      const char *mode, size_t modeLen, uint64_t freqHz,
                                      uint64_t flowStartSecs);

#endif
