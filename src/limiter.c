#include "pRepC.h"
#include "pRepCLimiter.h"
#include "system/systemHal.h"
#include "uthash.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    PREPC_BAND_INVALID,
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

typedef struct prepcRateEntry_t {
    char callsign[PREPC_RATE_CALLSIGN_MAX_LEN + 1];
    char mode[PREPC_RATE_MODE_MAX_LEN + 1];
    uint64_t lastReportSecs;
    uint64_t freqHz;

    struct prepcRateEntry_t *nextFree;

    UT_hash_handle hh;
} prepcRateEntry_t;

struct prepcRateCtx_t {
    prepcRateEntry_t *utTable;

    prepcRateEntry_t *entries;
    size_t maxEntries;

    prepcRateEntry_t *freeEntries;

    uint64_t lastPurged;
};

prepcBand_t prepc_band_classify(uint64_t freqHz) {
    if (freqHz == 0)
        return PREPC_BAND_INVALID;

    if (freqHz >= 135700 && freqHz <= 137800)
        return PREPC_BAND_2200M;

    if (freqHz >= 472000 && freqHz <= 479000)
        return PREPC_BAND_630M;

    if (freqHz >= 1800000 && freqHz <= 2000000)
        return PREPC_BAND_160M;

    if (freqHz >= 3500000 && freqHz <= 4000000)
        return PREPC_BAND_80M;

    if (freqHz >= 5351500 && freqHz <= 5366500)
        return PREPC_BAND_60M;

    if (freqHz >= 7000000 && freqHz <= 7300000)
        return PREPC_BAND_40M;

    if (freqHz >= 10100000 && freqHz <= 10150000)
        return PREPC_BAND_30M;

    if (freqHz >= 14000000 && freqHz <= 14350000)
        return PREPC_BAND_20M;

    if (freqHz >= 18068000 && freqHz <= 18168000)
        return PREPC_BAND_17M;

    if (freqHz >= 21000000 && freqHz <= 21450000)
        return PREPC_BAND_15M;

    if (freqHz >= 24890000 && freqHz <= 24990000)
        return PREPC_BAND_12M;

    if (freqHz >= 28000000 && freqHz <= 29700000)
        return PREPC_BAND_10M;

    if (freqHz >= 50000000 && freqHz <= 54000000)
        return PREPC_BAND_6M;

    if (freqHz >= 69900000 && freqHz <= 70500000)
        return PREPC_BAND_4M;

    if (freqHz >= 144000000 && freqHz <= 156000000)
        return PREPC_BAND_2M;

    if (freqHz >= 220000000 && freqHz <= 225000000)
        return PREPC_BAND_1_25M;

    if (freqHz >= 420000000 && freqHz <= 450000000)
        return PREPC_BAND_70CM;

    if (freqHz >= 902000000 && freqHz <= 928000000)
        return PREPC_BAND_33CM;

    if (freqHz >= 1240000000 && freqHz <= 1300000000)
        return PREPC_BAND_23CM;

    if (freqHz >= 2300000000 && freqHz <= 2450000000)
        return PREPC_BAND_13CM;

    if (freqHz >= 3300000000 && freqHz <= 3500000000)
        return PREPC_BAND_9CM;

    if (freqHz >= 5650000000 && freqHz <= 5925000000)
        return PREPC_BAND_5CM;

    if (freqHz >= 10000000000 && freqHz <= 15000000000)
        return PREPC_BAND_3CM;

    if (freqHz >= 24000000000 && freqHz <= 24250000000)
        return PREPC_BAND_1_2CM;

    if (freqHz >= 47000000000 && freqHz <= 47200000000)
        return PREPC_BAND_6MM;

    if (freqHz >= 76000000000 && freqHz <= 81500000000)
        return PREPC_BAND_4MM;

    if (freqHz >= 122250000000 && freqHz <= 123000000000)
        return PREPC_BAND_2_5MM;

    if (freqHz >= 134000000000 && freqHz <= 141000000000)
        return PREPC_BAND_2MM;

    if (freqHz >= 241000000000 && freqHz <= 250000000000)
        return PREPC_BAND_1MM;

    return PREPC_BAND_UNKNOWN;
}

prepcError_t prepc_rate_ctx_init(prepcRateCtx_t **ctxInput, size_t maxEntries) {
    if (!ctxInput || maxEntries == 0)
        return PREPC_ERR_INVALID_ARGS;

    *ctxInput = NULL;

    prepcRateCtx_t *ctx = malloc(sizeof(prepcRateCtx_t));
    if (!ctx)
        return PREPC_ERR_MEMORY;

    ctx->entries = NULL;
    ctx->freeEntries = NULL;
    ctx->utTable = NULL;
    ctx->maxEntries = 0;
    ctx->lastPurged = 0;

    uint64_t currentTime;
    if (!prepc_system_time_unix_u64(&currentTime)) {
        free(ctx);
        return PREPC_ERR_SYSTEM;
    }

    ctx->entries = calloc(maxEntries, sizeof(*ctx->entries));
    if (!ctx->entries) {
        free(ctx);
        return PREPC_ERR_MEMORY;
    }

    ctx->freeEntries = &ctx->entries[0];
    ctx->maxEntries = maxEntries;
    ctx->lastPurged = currentTime;

    for (size_t i = 0; i + 1 < maxEntries; ++i)
        ctx->entries[i].nextFree = &ctx->entries[i + 1];

    ctx->entries[maxEntries - 1].nextFree = NULL;

    *ctxInput = ctx;

    return PREPC_ERR_OK;
}

static prepcRateEntry_t *prepc_rate_find(prepcRateCtx_t *ctx, const char *callsign,
                                         size_t callsignLen) {
    if (!ctx || !callsign || callsignLen == 0)
        return NULL;

    prepcRateEntry_t *entry = NULL;

    HASH_FIND(hh, ctx->utTable, callsign, callsignLen, entry);

    return entry;
}

static prepcError_t prepc_rate_add(prepcRateCtx_t *ctx, const char *callsign, size_t callsignLen,
                                   const char *mode, size_t modeLen, uint64_t lastSend,
                                   uint64_t freqHz) {
    if (!ctx || !callsign || !mode)
        return PREPC_ERR_INVALID_ARGS;

    if (!ctx->freeEntries)
        return PREPC_ERR_FULL;

    prepcRateEntry_t *entry = ctx->freeEntries;

    ctx->freeEntries = entry->nextFree;
    entry->nextFree = NULL;

    memcpy(entry->callsign, callsign, callsignLen);
    entry->callsign[callsignLen] = '\0';

    memcpy(entry->mode, mode, modeLen);
    entry->mode[modeLen] = '\0';

    entry->lastReportSecs = lastSend;
    entry->freqHz = freqHz;

    HASH_ADD(hh, ctx->utTable, callsign, callsignLen, entry);

    return PREPC_ERR_OK;
}

static prepcError_t prepc_rate_update(prepcRateEntry_t *entry, const char *mode, size_t modeLen,
                                      uint64_t lastReportSecs, uint64_t freqHz) {
    if (!entry)
        return PREPC_ERR_INVALID_ARGS;

    if (modeLen != 0) {
        if (!mode)
            return PREPC_ERR_INVALID_ARGS;

        memcpy(entry->mode, mode, modeLen);
        entry->mode[modeLen] = '\0';
    }

    entry->lastReportSecs = lastReportSecs;
    entry->freqHz = freqHz;

    return PREPC_ERR_OK;
}

static void prepc_rate_delete(prepcRateCtx_t *ctx, prepcRateEntry_t *entry) {
    if (!ctx || !entry)
        return;

    HASH_DEL(ctx->utTable, entry);

    entry->nextFree = ctx->freeEntries;
    ctx->freeEntries = entry;
}

void prepc_rate_free(prepcRateCtx_t *ctx) {
    if (!ctx)
        return;

    if (ctx->entries && ctx->utTable) {
        prepcRateEntry_t *entry;
        prepcRateEntry_t *tmp;

        HASH_ITER(hh, ctx->utTable, entry, tmp) {
            HASH_DEL(ctx->utTable, entry);
        }
    }

    if (ctx->entries)
        free(ctx->entries);

    ctx->entries = NULL;
    ctx->freeEntries = NULL;
    ctx->utTable = NULL;
    ctx->maxEntries = 0;
    ctx->lastPurged = 0;

    free(ctx);
}

static prepcError_t prepc_rate_delete_oldest(prepcRateCtx_t *ctx) {
    if (!ctx)
        return PREPC_ERR_INVALID_ARGS;

    prepcRateEntry_t *entry;
    prepcRateEntry_t *tmp;
    prepcRateEntry_t *candidate = NULL;

    HASH_ITER(hh, ctx->utTable, entry, tmp) {
        if (!candidate || entry->lastReportSecs < candidate->lastReportSecs)
            candidate = entry;
    }

    if (!candidate)
        return PREPC_ERR_FULL;

    prepc_rate_delete(ctx, candidate);

    return PREPC_ERR_OK;
}

static prepcError_t prepc_rate_purge(prepcRateCtx_t *ctx, uint64_t currentTime,
                                     uint64_t timeoutSec) {
    if (!ctx || timeoutSec == 0)
        return PREPC_ERR_INVALID_ARGS;

    prepcRateEntry_t *entry;
    prepcRateEntry_t *tmp;

    HASH_ITER(hh, ctx->utTable, entry, tmp) {
        if ((currentTime - entry->lastReportSecs) >= timeoutSec)
            prepc_rate_delete(ctx, entry);
    }

    return PREPC_ERR_OK;
}

prepcError_t prepc_rate_should_report(prepcRateCtx_t *ctx, const char *callsign, size_t callsignLen,
                                      const char *mode, size_t modeLen, uint64_t freqHz,
                                      uint64_t flowStartSecs) {
    if (!ctx || !callsign || !mode)
        return PREPC_ERR_INVALID_ARGS;

    if (callsignLen == 0 || callsignLen > PREPC_RATE_CALLSIGN_MAX_LEN)
        return PREPC_ERR_INVALID_ARGS;

    if (modeLen == 0 || modeLen > PREPC_RATE_MODE_MAX_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepcError_t rc;
    prepcRateEntry_t *entry = NULL;

    uint64_t currentTime;
    if (!prepc_system_time_unix_u64(&currentTime))
        return PREPC_ERR_SYSTEM;

    if (flowStartSecs > currentTime)
        return PREPC_ERR_INVALID_ARGS;

    if ((currentTime - ctx->lastPurged) >= PREPC_RATE_PURGE_INTERVAL_S) {
        rc = prepc_rate_purge(ctx, currentTime, PREPC_RATE_MAX_AGE_S);
        if (rc != PREPC_ERR_OK)
            return rc;

        ctx->lastPurged = currentTime;
    }

    entry = prepc_rate_find(ctx, callsign, callsignLen);
    if (!entry) {
        rc = prepc_rate_add(ctx, callsign, callsignLen, mode, modeLen, flowStartSecs, freqHz);
        if (rc == PREPC_ERR_OK)
            return PREPC_ERR_OK;

        if (rc != PREPC_ERR_FULL)
            return rc;

        // Pool is full
        rc = prepc_rate_delete_oldest(ctx);
        if (rc != PREPC_ERR_OK)
            return rc;

        return prepc_rate_add(ctx, callsign, callsignLen, mode, modeLen, flowStartSecs, freqHz);
    }

    if (entry->lastReportSecs > flowStartSecs)
        return PREPC_ERR_NOT_REPORT;

    // It has been reported recently
    if ((flowStartSecs - entry->lastReportSecs) < PREPC_RATE_MIN_INTERVAL_S)
        return PREPC_ERR_NOT_REPORT;

    prepcBand_t bandNew = prepc_band_classify(freqHz);
    prepcBand_t bandOld = prepc_band_classify(entry->freqHz);

    bool changeDetected = false;
    // Mode has changed
    if (strlen(entry->mode) != modeLen || memcmp(entry->mode, mode, modeLen) != 0)
        changeDetected = true;
    // Band has changed
    if (bandOld != bandNew && bandNew != PREPC_BAND_INVALID)
        changeDetected = true;
    // Unknown band, check frequency difference
    if (bandOld == PREPC_BAND_UNKNOWN && bandNew == PREPC_BAND_UNKNOWN) {
        uint64_t diffHz;
        if (entry->freqHz == freqHz)
            diffHz = 0;
        else if (entry->freqHz < freqHz)
            diffHz = freqHz - entry->freqHz;
        else
            diffHz = entry->freqHz - freqHz;

        if (diffHz >= PREPC_RATE_UNKNOWN_FREQ_CHANGE_HZ)
            changeDetected = true;
    }

    // Data has not changed and we wait
    if ((flowStartSecs - entry->lastReportSecs) < PREPC_RATE_UNCHANGED_INTERVAL_S && !changeDetected)
        return PREPC_ERR_NOT_REPORT;

    // Data has changed so report it
    rc = prepc_rate_update(entry, mode, modeLen, flowStartSecs, freqHz);
    if (rc != PREPC_ERR_OK)
        return rc;

    return PREPC_ERR_OK;
}
