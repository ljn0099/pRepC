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

typedef enum {
    PREPC_BAND_UNKNOWN,
    PREPC_BAND_2200M,
    PREPC_BAND_630M,
    PREPC_BAND_160M,
    PREPC_BAND_80M,
    PREPC_BAND_60M,
    PREPC_BAND_40M,
    PREPC_BAND_30M,
    PREPC_BAND_20M,
    PREPC_BAND_17M,
    PREPC_BAND_15M,
    PREPC_BAND_12M,
    PREPC_BAND_10M,
    PREPC_BAND_6M,
    PREPC_BAND_4M,
    PREPC_BAND_2M,
    PREPC_BAND_1_25M,
    PREPC_BAND_70CM,
    PREPC_BAND_33CM,
    PREPC_BAND_23CM,
    PREPC_BAND_13CM,
    PREPC_BAND_9CM,
    PREPC_BAND_5CM,
    PREPC_BAND_3CM,
    PREPC_BAND_1_2CM,
    PREPC_BAND_6MM,
    PREPC_BAND_4MM,
    PREPC_BAND_2_5MM,
    PREPC_BAND_2MM,
    PREPC_BAND_1MM
} prepcBand_t;

typedef struct prepcRateEntry_t prepcRateEntry_t;

typedef struct {
    prepcRateEntry_t *utTable;

    prepcRateEntry_t *entries;
    size_t maxEntries;

    prepcRateEntry_t *freeEntries;

    uint64_t lastPurged;
} prepcRateCtx_t;

prepcError_t prepc_rate_ctx_init(prepcRateCtx_t *ctx, size_t maxEntries);

void prepc_rate_free(prepcRateCtx_t *ctx);

prepcError_t prepc_rate_should_report(prepcRateCtx_t *ctx, const char *callsign, size_t callsignLen,
                                      const char *mode, size_t modeLen, uint64_t freqHz);

#endif
