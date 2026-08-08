#include "binio.h"
#include "pRepC.h"
#include "system/systemHal.h"
#include "time.h"
#include "udp/udpHal.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PREPC_MAX_STR_LEN 254

#define PREPC_IPFIX_VERSION 0x000A

#define PREPC_RECEIVER_DATA_FLAG 0x0003
#define PREPC_RECEIVER_SCOPE_FIELDS_COUNT 0x0001

#define PREPC_SENDER_DATA_FLAG 0x0002

#define PREPC_ENTERPRISE_NUM 0x0000768F

#define PREPC_VARIABLE_STR 0xFFFF

#define PREPC_ENTERPRISE_MASK 0x8000

#define PREPC_PADDING_DIVISOR 4

#define PREPC_DATA_HEADER_LEN 2

#define PREPC_FIELD_MASK(field) ((prepcFields_t)1u << (field))

#define PREPC_INITIAL_TEMPLATE_NUM 256
#define PREPC_FINAL_TEMPLATE_NUM 65535

#define PREPC_RFD_NUM_STARTUP 5
#define PREPC_RFD_SEC_TIMEOUT 2700 // 45 minutes

#define PREPC_DNS_TTL_SEC 300 // 5 minutes

#define PREPC_RECEIVER_MANDATORY_FIELDS_MASK                                                       \
    (PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_CALLSIGN) |                                             \
     PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_LOCATOR) |                                              \
     PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_DECODER_SOFTWARE))

#define PREPC_SENDER_MANDATORY_FIELDS_MASK                                                         \
    (PREPC_FIELD_MASK(PREPC_SENDER_FIELD_CALLSIGN) | PREPC_FIELD_MASK(PREPC_SENDER_FIELD_MODE) |   \
     PREPC_FIELD_MASK(PREPC_SENDER_FIELD_INFO_SRC) |                                               \
     PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FLOW_START_SECS))

static const uint16_t receiverFieldIds[PREPC_RECEIVER_FIELD_COUNT] = {
    [PREPC_RECEIVER_FIELD_CALLSIGN] = 0x8002,         [PREPC_RECEIVER_FIELD_LOCATOR] = 0x8004,
    [PREPC_RECEIVER_FIELD_DECODER_SOFTWARE] = 0x8008, [PREPC_RECEIVER_FIELD_ANTENNA_INFO] = 0x8009,
    [PREPC_RECEIVER_FIELD_PERSISTENT_ID] = 0x800C,    [PREPC_RECEIVER_FIELD_RIG_INFO] = 0x800D,
};

static const uint16_t senderFieldIds[PREPC_SENDER_FIELD_COUNT] = {
    [PREPC_SENDER_FIELD_CALLSIGN] = 0x8001,
    [PREPC_SENDER_FIELD_LOCATOR] = 0x8003,
    [PREPC_SENDER_FIELD_FREQUENCY] = 0x8005,
    [PREPC_SENDER_FIELD_SNR] = 0x8006,
    [PREPC_SENDER_FIELD_IMD] = 0x8007,
    [PREPC_SENDER_FIELD_MODE] = 0x800A,
    [PREPC_SENDER_FIELD_INFO_SRC] = 0x800B,
    [PREPC_SENDER_FIELD_FLOW_START_SECS] = 0x0096,
    [PREPC_SENDER_FIELD_MESSAGE_BITS] = 0x800E,
    [PREPC_SENDER_FIELD_DELTA_TIME] = 0x800F,
    [PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY] = 0x8010,
};

#define PREPC_INFO_SRC_TEST_MASK 0x80

typedef enum { PREPC_SET_SENDER = 0, PREPC_SET_RECEIVER } prepcSetType_t;

#ifdef DEBUG

#define DEBUG_printf(fmt, ...)                                                                     \
    do {                                                                                           \
        printf("[DEBUG][TESTLIB][%s] " fmt "\n", __func__, ##__VA_ARGS__);                         \
    } while (0)

#else

#define DEBUG_printf(...)                                                                          \
    do {                                                                                           \
    } while (0)
#endif

bool check_locator(const char *locator, size_t len) {
    // Must be 4, 6, or 8 characters
    if (len != 4 && len != 6 && len != 8)
        return false;

    // 1st pair: letters A-R
    if (!(locator[0] >= 'A' && locator[0] <= 'R'))
        return false;
    if (!(locator[1] >= 'A' && locator[1] <= 'R'))
        return false;

    // 2nd pair: digits 0-9
    if (!isdigit(locator[2]))
        return false;
    if (!isdigit(locator[3]))
        return false;

    // Optional 3rd pair
    if (len >= 6) {
        if (!(locator[4] >= 'A' && locator[4] <= 'X'))
            return false;
        if (!(locator[5] >= 'A' && locator[5] <= 'X'))
            return false;
    }

    // Optional 4th pair
    if (len == 8) {
        if (!isdigit(locator[6]))
            return false;
        if (!isdigit(locator[7]))
            return false;
    }

    return true;
}

static inline size_t prepc_popcount(prepcFields_t value) {
    size_t count = 0;

    while (value) {
        value &= value - 1;
        ++count;
    }

    return count;
}

static inline bool prepc_has_space(const prepcBuf_t *buf, size_t bytes) {
    return (buf->maxLen - buf->len >= bytes);
}

static inline size_t prepc_append_padding(prepcBuf_t *buf, size_t written) {
    size_t paddingBytes =
        (PREPC_PADDING_DIVISOR - (written % PREPC_PADDING_DIVISOR)) % PREPC_PADDING_DIVISOR;

    if (buf)
        buf->len += binio_pad_bytes(buf->data + buf->len, 0x00, paddingBytes);

    return paddingBytes;
}

static inline size_t prepc_append_u8(prepcBuf_t *buf, uint8_t value) {
    if (buf)
        buf->len += binio_write_u8(buf->data + buf->len, value);

    return sizeof(uint8_t);
}

static inline size_t prepc_append_u16(prepcBuf_t *buf, uint16_t value) {
    if (buf)
        buf->len += binio_write_u16_be(buf->data + buf->len, value);

    return sizeof(uint16_t);
}

static inline size_t prepc_append_u32(prepcBuf_t *buf, uint32_t value) {
    if (buf)
        buf->len += binio_write_u32_be(buf->data + buf->len, value);

    return sizeof(uint32_t);
}

static inline size_t prepc_append_u64(prepcBuf_t *buf, uint64_t value) {
    if (buf)
        buf->len += binio_write_u64_be(buf->data + buf->len, value);

    return sizeof(uint64_t);
}

static inline size_t prepc_append_var_uint(prepcBuf_t *buf, uint64_t value, size_t nBytes) {
    if (nBytes > sizeof(uint64_t))
        nBytes = sizeof(uint64_t);

    if (buf)
        buf->len += binio_write_u64_trunc_be(buf->data + buf->len, value, nBytes);

    return nBytes;
}

static inline size_t prepc_append_var_int(prepcBuf_t *buf, int64_t value, size_t nBytes) {
    if (nBytes > sizeof(int64_t))
        nBytes = sizeof(int64_t);

    if (buf)
        buf->len += binio_write_i64_trunc_be(buf->data + buf->len, value, nBytes);

    return nBytes;
}

static inline size_t prepc_uint_min_bytes(uint64_t value) {
    if (value <= 0xFFULL)
        return 1;
    if (value <= 0xFFFFULL)
        return 2;
    if (value <= 0xFFFFFFULL)
        return 3;
    if (value <= 0xFFFFFFFFULL)
        return 4;
    if (value <= 0xFFFFFFFFFFULL)
        return 5;
    if (value <= 0xFFFFFFFFFFFFULL)
        return 6;
    if (value <= 0xFFFFFFFFFFFFFFULL)
        return 7;

    return 8;
}

static inline size_t prepc_int_min_bytes(int64_t value) {
    if (value >= -0x80LL && value <= 0x7FLL)
        return 1;
    if (value >= -0x8000LL && value <= 0x7FFFLL)
        return 2;
    if (value >= -0x800000LL && value <= 0x7FFFFFLL)
        return 3;
    if (value >= -0x80000000LL && value <= 0x7FFFFFFFLL)
        return 4;
    if (value >= -0x8000000000LL && value <= 0x7FFFFFFFFFLL)
        return 5;
    if (value >= -0x800000000000LL && value <= 0x7FFFFFFFFFFFLL)
        return 6;
    if (value >= -0x80000000000000LL && value <= 0x7FFFFFFFFFFFFFLL)
        return 7;

    return 8;
}

static inline size_t prepc_append_bytes(prepcBuf_t *buf, const uint8_t *bytes, size_t nBytes) {
    if (buf)
        buf->len += binio_write_bytes(buf->data + buf->len, bytes, nBytes);

    return nBytes;
}

static inline size_t prepc_append_str(prepcBuf_t *buf, const char *str, size_t strLen) {
    if (strLen >= PREPC_MAX_STR_LEN)
        strLen = PREPC_MAX_STR_LEN;

    if (buf) {
        buf->len += binio_write_u8(buf->data + buf->len, (uint8_t)strLen);
        buf->len += binio_write_bytes(buf->data + buf->len, (const uint8_t *)str, strLen);
    }

    return sizeof(uint8_t) + strLen;
}

static size_t prepc_encode_receiver_data(prepcBuf_t *buf, const prepcReceiverData_t *receiverData,
                                         uint16_t templateId, size_t receiverDataLen) {
    size_t written = 0;

    // 0. Header
    written += prepc_append_u16(buf, (uint16_t)templateId);
    written += prepc_append_u16(buf, (uint16_t)receiverDataLen);

    // 1. Callsign
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_CALLSIGN))
        written += prepc_append_str(buf, receiverData->callsign,
                                    receiverData->lengths[PREPC_RECEIVER_FIELD_CALLSIGN]);

    // 2. Locator
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_LOCATOR))
        written += prepc_append_str(buf, receiverData->locator,
                                    receiverData->lengths[PREPC_RECEIVER_FIELD_LOCATOR]);

    // 3. Decoder Software
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_DECODER_SOFTWARE))
        written += prepc_append_str(buf, receiverData->decoderSoftware,
                                    receiverData->lengths[PREPC_RECEIVER_FIELD_DECODER_SOFTWARE]);

    // 4. Antenna Information
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_ANTENNA_INFO))
        written += prepc_append_str(buf, receiverData->antennaInfo,
                                    receiverData->lengths[PREPC_RECEIVER_FIELD_ANTENNA_INFO]);

    // 5. Persistend Identifier
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_PERSISTENT_ID))
        written += prepc_append_str(buf, receiverData->persistentId,
                                    receiverData->lengths[PREPC_RECEIVER_FIELD_PERSISTENT_ID]);

    // 6. Rig Information
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_RIG_INFO))
        written += prepc_append_str(buf, receiverData->rigInfo,
                                    receiverData->lengths[PREPC_RECEIVER_FIELD_RIG_INFO]);

    written += prepc_append_padding(buf, written);

    return written;
}

static size_t prepc_encode_sender_data(prepcBuf_t *buf, const prepcSenderData_t *senderData,
                                       uint16_t templateId, size_t senderDataLen) {
    size_t written = 0;

    // 0. Header
    written += prepc_append_u16(buf, (uint16_t)templateId);
    written += prepc_append_u16(buf, (uint16_t)senderDataLen);

    // 1. Callsign
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_CALLSIGN))
        written += prepc_append_str(buf, senderData->callsign,
                                    senderData->lengths[PREPC_SENDER_FIELD_CALLSIGN]);

    // 2. Locator
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_LOCATOR))
        written += prepc_append_str(buf, senderData->locator,
                                    senderData->lengths[PREPC_SENDER_FIELD_LOCATOR]);

    // 3. Frequency
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FREQUENCY))
        written += prepc_append_var_uint(buf, senderData->frequency,
                                         senderData->lengths[PREPC_SENDER_FIELD_FREQUENCY]);

    // 4. SNR
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_SNR))
        written +=
            prepc_append_var_int(buf, senderData->snr, senderData->lengths[PREPC_SENDER_FIELD_SNR]);

    // 5. IMD
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_IMD))
        written +=
            prepc_append_var_int(buf, senderData->imd, senderData->lengths[PREPC_SENDER_FIELD_IMD]);

    // 6. Mode
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_MODE))
        written +=
            prepc_append_str(buf, senderData->mode, senderData->lengths[PREPC_SENDER_FIELD_MODE]);

    // 7. Information Source
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_INFO_SRC))
        written += prepc_append_var_uint(buf, senderData->infoSrc,
                                         senderData->lengths[PREPC_SENDER_FIELD_INFO_SRC]);

    // 8. Flow Starts Seconds
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FLOW_START_SECS))
        written += prepc_append_var_uint(buf, senderData->flowStartSecs,
                                         senderData->lengths[PREPC_SENDER_FIELD_FLOW_START_SECS]);

    // 9. Message Bits
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_MESSAGE_BITS))
        written += prepc_append_bytes(buf, senderData->messageBits,
                                      senderData->lengths[PREPC_SENDER_FIELD_MESSAGE_BITS]);

    // 10. Delta Time
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_DELTA_TIME))
        written += prepc_append_var_int(buf, senderData->deltaTime,
                                        senderData->lengths[PREPC_SENDER_FIELD_DELTA_TIME]);

    // 11. Fractional Frequency
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY))
        written +=
            prepc_append_var_uint(buf, senderData->fractionalFrequency,
                                  senderData->lengths[PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY]);

    written += prepc_append_padding(buf, written);

    return written;
}

prepcError_t prepc_append_receiver_data(prepcBuf_t *buf, const prepcReceiverData_t *receiverData,
                                        uint16_t templateId) {
    if (!buf || !receiverData)
        return PREPC_ERR_INVALID_ARGS;

    size_t receiverDataLen = prepc_encode_receiver_data(NULL, receiverData, templateId, 0);

    if (!prepc_has_space(buf, receiverDataLen))
        return PREPC_ERR_BUF_TOO_SMALL;

    prepc_encode_receiver_data(buf, receiverData, templateId, receiverDataLen);

    return PREPC_ERR_OK;
}

prepcError_t prepc_append_sender_data(prepcBuf_t *buf, const prepcSenderData_t *senderData,
                                      uint16_t templateId) {
    if (!buf || !senderData)
        return PREPC_ERR_INVALID_ARGS;

    size_t senderDataLen = prepc_encode_sender_data(NULL, senderData, templateId, 0);

    if (!prepc_has_space(buf, senderDataLen))
        return PREPC_ERR_BUF_TOO_SMALL;

    prepc_encode_sender_data(buf, senderData, templateId, senderDataLen);

    return PREPC_ERR_OK;
}

// RFD Header Format
// The ScopeFieldsCount is only used for the receiver rfd
// <SetId:2><RFDLen:2><TemplateId:2><NumFields:2>[ScopeFieldsNum:2]

// Fields Definition Format
// If the FieldId has the first bit at 1 (0x8000 mask) its an enterprise field.
// If is a var string the FieldLen is set to 0xFFFF
// Standard Field
// <FieldId:2><FieldLen:2>
// Enterprise Field
// <FieldId:2><FieldLen:2><EnterpriseNum:4>

static inline size_t prepc_append_field_rfd(prepcBuf_t *buf, const uint16_t *fieldIds,
                                            unsigned field, uint16_t len) {
    uint16_t id = fieldIds[field];

    size_t written = 0;

    written += prepc_append_u16(buf, id);
    written += prepc_append_u16(buf, len);

    if (id & PREPC_ENTERPRISE_MASK)
        written += prepc_append_u32(buf, PREPC_ENTERPRISE_NUM);

    return written;
}

static size_t prepc_encode_receiver_rfd(prepcBuf_t *buf, const prepcReceiverData_t *receiverData,
                                        uint16_t templateId, size_t receiverRfdLen,
                                        uint8_t *rfdKey) {
    uint8_t *keyP = NULL;
    if (rfdKey) {
        keyP = rfdKey;
        binio_pad_bytes(keyP, 0x00, PREPC_TEMPLATE_KEY_LEN);

        keyP +=
            binio_write_bytes(keyP, (uint8_t *)&receiverData->fields, sizeof(receiverData->fields));
    }

    size_t written = 0;

    // 0. Header
    written += prepc_append_u16(buf, (uint16_t)PREPC_RECEIVER_DATA_FLAG);
    written += prepc_append_u16(buf, (uint16_t)receiverRfdLen);
    written += prepc_append_u16(buf, templateId);
    written += prepc_append_u16(buf, (uint16_t)prepc_popcount(receiverData->fields));
    written += prepc_append_u16(buf, (uint16_t)PREPC_RECEIVER_SCOPE_FIELDS_COUNT);

    // 1. Callsign
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_CALLSIGN)) {
        written += prepc_append_field_rfd(buf, receiverFieldIds, PREPC_RECEIVER_FIELD_CALLSIGN,
                                          PREPC_VARIABLE_STR);
    }

    // 2. Locator
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_LOCATOR)) {
        written += prepc_append_field_rfd(buf, receiverFieldIds, PREPC_RECEIVER_FIELD_LOCATOR,
                                          PREPC_VARIABLE_STR);
    }

    // 3. Decoder Software
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_DECODER_SOFTWARE)) {
        written += prepc_append_field_rfd(
            buf, receiverFieldIds, PREPC_RECEIVER_FIELD_DECODER_SOFTWARE, PREPC_VARIABLE_STR);
    }

    // 4. Antenna Info
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_ANTENNA_INFO)) {
        written += prepc_append_field_rfd(buf, receiverFieldIds, PREPC_RECEIVER_FIELD_ANTENNA_INFO,
                                          PREPC_VARIABLE_STR);
    }

    // 5. Persistent Identifier
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_PERSISTENT_ID)) {
        written += prepc_append_field_rfd(buf, receiverFieldIds, PREPC_RECEIVER_FIELD_PERSISTENT_ID,
                                          PREPC_VARIABLE_STR);
    }

    // 6. Persistent Identifier
    if (receiverData->fields & PREPC_FIELD_MASK(PREPC_RECEIVER_FIELD_RIG_INFO)) {
        written += prepc_append_field_rfd(buf, receiverFieldIds, PREPC_RECEIVER_FIELD_RIG_INFO,
                                          PREPC_VARIABLE_STR);
    }

    written += prepc_append_padding(buf, written);

    return written;
}

static size_t prepc_encode_sender_rfd(prepcBuf_t *buf, const prepcSenderData_t *senderData,
                                      uint16_t templateId, size_t senderRfdLen, uint8_t *rfdKey) {
    uint8_t *keyP = NULL;
    if (rfdKey) {
        keyP = rfdKey;
        binio_pad_bytes(keyP, 0x00, PREPC_TEMPLATE_KEY_LEN);

        keyP += binio_write_bytes(keyP, (uint8_t *)&senderData->fields, sizeof(senderData->fields));
    }

    size_t written = 0;

    // 0. Header
    written += prepc_append_u16(buf, (uint16_t)PREPC_SENDER_DATA_FLAG);
    written += prepc_append_u16(buf, (uint16_t)senderRfdLen);
    written += prepc_append_u16(buf, templateId);
    written += prepc_append_u16(buf, (uint16_t)prepc_popcount(senderData->fields));

    // 1. Callsign
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_CALLSIGN)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_CALLSIGN,
                                          PREPC_VARIABLE_STR);
    }

    // 2. Locator
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_LOCATOR)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_LOCATOR,
                                          PREPC_VARIABLE_STR);
    }

    // 3. Frequency
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FREQUENCY)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_FREQUENCY,
                                          senderData->lengths[PREPC_SENDER_FIELD_FREQUENCY]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_FREQUENCY]);
    }

    // 4. SNR
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_SNR)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_SNR,
                                          senderData->lengths[PREPC_SENDER_FIELD_SNR]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_SNR]);
    }

    // 5. IMD
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_IMD)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_IMD,
                                          senderData->lengths[PREPC_SENDER_FIELD_IMD]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_IMD]);
    }

    // 6. Mode
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_MODE)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_MODE,
                                          PREPC_VARIABLE_STR);
    }

    // 7. Information Source
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_INFO_SRC)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_INFO_SRC,
                                          senderData->lengths[PREPC_SENDER_FIELD_INFO_SRC]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_INFO_SRC]);
    }

    // 8. Flow Start Seconds
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FLOW_START_SECS)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_FLOW_START_SECS,
                                          senderData->lengths[PREPC_SENDER_FIELD_FLOW_START_SECS]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_FLOW_START_SECS]);
    }

    // 9. Message Bits
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_MESSAGE_BITS)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_MESSAGE_BITS,
                                          senderData->lengths[PREPC_SENDER_FIELD_MESSAGE_BITS]);
    }

    // 10. Delta Time
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_DELTA_TIME)) {
        written += prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_DELTA_TIME,
                                          senderData->lengths[PREPC_SENDER_FIELD_DELTA_TIME]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_DELTA_TIME]);
    }

    // 11. Fractional Frequency
    if (senderData->fields & PREPC_FIELD_MASK(PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY)) {
        written +=
            prepc_append_field_rfd(buf, senderFieldIds, PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY,
                                   senderData->lengths[PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY]);

        if (keyP)
            keyP +=
                binio_write_u8(keyP, senderData->lengths[PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY]);
    }

    written += prepc_append_padding(buf, written);

    return written;
}

prepcError_t prepc_append_receiver_rfd(prepcBuf_t *buf, const prepcReceiverData_t *receiverData,
                                       uint16_t templateId) {
    if (!buf || !receiverData)
        return PREPC_ERR_INVALID_ARGS;

    size_t receiverRfdLen = prepc_encode_receiver_rfd(NULL, receiverData, templateId, 0, NULL);

    if (!prepc_has_space(buf, receiverRfdLen))
        return PREPC_ERR_BUF_TOO_SMALL;

    prepc_encode_receiver_rfd(buf, receiverData, templateId, receiverRfdLen, NULL);

    return PREPC_ERR_OK;
}

prepcError_t prepc_append_sender_rfd(prepcBuf_t *buf, const prepcSenderData_t *senderData,
                                     uint16_t templateId) {
    if (!buf || !senderData)
        return PREPC_ERR_INVALID_ARGS;

    size_t senderRfdLen = prepc_encode_sender_rfd(NULL, senderData, templateId, 0, NULL);

    if (!prepc_has_space(buf, senderRfdLen))
        return PREPC_ERR_BUF_TOO_SMALL;

    prepc_encode_sender_rfd(buf, senderData, templateId, senderRfdLen, NULL);

    return PREPC_ERR_OK;
}

prepcError_t prepc_generate_receiver_rfd_key(const prepcReceiverData_t *receiverData,
                                             uint8_t *rfdKey) {
    if (!receiverData || !rfdKey)
        return PREPC_ERR_INVALID_ARGS;

    prepc_encode_receiver_rfd(NULL, receiverData, 0, 0, rfdKey);

    return PREPC_ERR_OK;
}

prepcError_t prepc_generate_sender_rfd_key(const prepcSenderData_t *senderData, uint8_t *rfdKey) {
    if (!senderData || !rfdKey)
        return PREPC_ERR_INVALID_ARGS;

    prepc_encode_sender_rfd(NULL, senderData, 0, 0, rfdKey);

    return PREPC_ERR_OK;
}

prepcError_t prepc_write_packet_header(prepcBuf_t *buf, size_t datagramLen, uint32_t txTimestamp,
                                       uint32_t sequenceNum, uint32_t sessionId) {
    // Header Format (16 bytes total)
    // <IpfixVersion:2><DatagramLen:2><TxTimestamp:4><SequenceNum:4><SessionId:4>

    if (buf->maxLen < 16)
        return PREPC_ERR_BUF_TOO_SMALL;

    if (buf->len < 16)
        buf->len = 16;

    uint8_t *p = buf->data;

    p += binio_write_u16_be(p, (uint16_t)PREPC_IPFIX_VERSION);
    p += binio_write_u16_be(p, (uint16_t)datagramLen);
    p += binio_write_u32_be(p, txTimestamp);
    p += binio_write_u32_be(p, sequenceNum);
    p += binio_write_u32_be(p, sessionId);

    return PREPC_ERR_OK;
}

prepcError_t prepc_buf_init(prepcBuf_t *buf) {
    buf->len = 0;
    buf->maxLen = PREPC_PACKET_LEN;

    return PREPC_ERR_OK;
}

void prepc_buf_reset(prepcBuf_t *buf) {
    buf->len = 0;
}

static inline void prepc_set_str(uint32_t *fields, uint8_t *lengths, unsigned field,
                                 const char **dst, const char *value, size_t len) {
    *fields |= PREPC_FIELD_MASK(field);
    *dst = value;
    lengths[field] = (uint8_t)len;
}

static inline void prepc_receiver_set_str(prepcReceiverData_t *receiverData,
                                          prepcReceiverField_t field, const char **dst,
                                          const char *value, size_t len) {
    prepc_set_str(&receiverData->fields, receiverData->lengths, field, dst, value, len);
}

static inline void prepc_sender_set_str(prepcSenderData_t *senderData, prepcSenderField_t field,
                                        const char **dst, const char *value, size_t len) {
    prepc_set_str(&senderData->fields, senderData->lengths, field, dst, value, len);
}

static inline void prepc_set_int(uint32_t *fields, uint8_t *lengths, unsigned field, int64_t *dst,
                                 int64_t value, size_t defLen, bool fixedLen) {
    *fields |= PREPC_FIELD_MASK(field);
    *dst = value;

    size_t len = defLen;

    if (!fixedLen) {
        len = prepc_int_min_bytes(value);

        if (len < defLen)
            len = defLen;
    }

    lengths[field] = (uint8_t)len;
}

static inline void prepc_set_uint(uint32_t *fields, uint8_t *lengths, unsigned field, uint64_t *dst,
                                  uint64_t value, size_t defLen, bool fixedLen) {
    *fields |= PREPC_FIELD_MASK(field);
    *dst = value;

    size_t len = defLen;

    if (!fixedLen) {
        len = prepc_uint_min_bytes(value);

        if (len < defLen)
            len = defLen;
    }

    lengths[field] = (uint8_t)len;
}

static inline void prepc_sender_set_uint(prepcSenderData_t *senderData, prepcSenderField_t field,
                                         uint64_t *dst, uint64_t value, size_t defLen,
                                         bool fixedLen) {
    prepc_set_uint(&senderData->fields, senderData->lengths, field, dst, value, defLen, fixedLen);
}

static inline void prepc_sender_set_int(prepcSenderData_t *senderData, prepcSenderField_t field,
                                        int64_t *dst, int64_t value, size_t defLen, bool fixedLen) {
    prepc_set_int(&senderData->fields, senderData->lengths, field, dst, value, defLen, fixedLen);
}

static inline void prepc_set_bytes(uint32_t *fields, uint8_t *lengths, unsigned field,
                                   const uint8_t **dst, const uint8_t *value, size_t len) {
    *fields |= PREPC_FIELD_MASK(field);
    *dst = value;
    lengths[field] = (uint8_t)len;
}

static inline void prepc_sender_set_bytes(prepcSenderData_t *senderData, prepcSenderField_t field,
                                          const uint8_t **dst, const uint8_t *value, size_t len) {
    prepc_set_bytes(&senderData->fields, senderData->lengths, field, dst, value, len);
}

prepcError_t prepc_receiver_data_set_callsign(prepcReceiverData_t *receiverData,
                                              const char *callsign, size_t len) {
    if (!receiverData || !callsign)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_receiver_set_str(receiverData, PREPC_RECEIVER_FIELD_CALLSIGN, &receiverData->callsign,
                           callsign, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_receiver_data_set_locator(prepcReceiverData_t *receiverData, const char *locator,
                                             size_t len) {
    if (!receiverData || !locator)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    if (!check_locator(locator, len))
        return PREPC_ERR_INVALID_ARGS;

    prepc_receiver_set_str(receiverData, PREPC_RECEIVER_FIELD_LOCATOR, &receiverData->locator,
                           locator, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_receiver_data_set_decoder_software(prepcReceiverData_t *receiverData,
                                                      const char *decoderSoftware, size_t len) {
    if (!receiverData || !decoderSoftware)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_receiver_set_str(receiverData, PREPC_RECEIVER_FIELD_DECODER_SOFTWARE,
                           &receiverData->decoderSoftware, decoderSoftware, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_receiver_data_set_antenna_info(prepcReceiverData_t *receiverData,
                                                  const char *antennaInfo, size_t len) {
    if (!receiverData || !antennaInfo)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_receiver_set_str(receiverData, PREPC_RECEIVER_FIELD_ANTENNA_INFO,
                           &receiverData->antennaInfo, antennaInfo, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_receiver_data_set_persistent_id(prepcReceiverData_t *receiverData,
                                                   const char *persistentId, size_t len) {
    if (!receiverData || !persistentId)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_receiver_set_str(receiverData, PREPC_RECEIVER_FIELD_PERSISTENT_ID,
                           &receiverData->persistentId, persistentId, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_receiver_data_set_rig_info(prepcReceiverData_t *receiverData,
                                              const char *rigInfo, size_t len) {
    if (!receiverData || !rigInfo)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_receiver_set_str(receiverData, PREPC_RECEIVER_FIELD_RIG_INFO, &receiverData->rigInfo,
                           rigInfo, len);

    return PREPC_ERR_OK;
}

void prepc_receiver_data_reset(prepcReceiverData_t *receiverData) {
    if (!receiverData)
        return;

    memset(receiverData, 0, sizeof(*receiverData));
}

prepcError_t prepc_sender_data_set_callsign(prepcSenderData_t *senderData, const char *callsign,
                                            size_t len) {
    if (!senderData || !callsign)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_str(senderData, PREPC_SENDER_FIELD_CALLSIGN, &senderData->callsign, callsign,
                         len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_locator(prepcSenderData_t *senderData, const char *locator,
                                           size_t len) {
    if (!senderData || !locator)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    if (!check_locator(locator, len))
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_str(senderData, PREPC_SENDER_FIELD_LOCATOR, &senderData->locator, locator,
                         len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_frequency(prepcSenderData_t *senderData, uint64_t frequency) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_uint(senderData, PREPC_SENDER_FIELD_FREQUENCY, &senderData->frequency,
                          frequency, 4, false);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_snr(prepcSenderData_t *senderData, int64_t snr) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_int(senderData, PREPC_SENDER_FIELD_SNR, &senderData->snr, snr, 1, false);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_imd(prepcSenderData_t *senderData, int64_t imd) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_int(senderData, PREPC_SENDER_FIELD_IMD, &senderData->imd, imd, 1, false);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_mode(prepcSenderData_t *senderData, const char *mode,
                                        size_t len) {
    if (!senderData || !mode)
        return PREPC_ERR_INVALID_ARGS;

    if (len > PREPC_MAX_STR_LEN)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_str(senderData, PREPC_SENDER_FIELD_MODE, &senderData->mode, mode, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_info_src(prepcSenderData_t *senderData, prepcInfoSrc_t infoSrc,
                                            bool testTransmission) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    uint64_t infoSrcVal = infoSrc;
    if (testTransmission)
        infoSrcVal |= PREPC_INFO_SRC_TEST_MASK;

    prepc_sender_set_uint(senderData, PREPC_SENDER_FIELD_INFO_SRC, &senderData->infoSrc, infoSrcVal,
                          1, true);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_flow_start_secs(prepcSenderData_t *senderData,
                                                   uint64_t flowStartSecs) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_uint(senderData, PREPC_SENDER_FIELD_FLOW_START_SECS,
                          &senderData->flowStartSecs, flowStartSecs, 4, false);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_message_bits(prepcSenderData_t *senderData, const uint8_t *bytes,
                                                size_t len) {
    if (!senderData || !bytes)
        return PREPC_ERR_INVALID_ARGS;

    prepc_sender_set_bytes(senderData, PREPC_SENDER_FIELD_MESSAGE_BITS, &senderData->messageBits,
                           bytes, len);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_delta_time(prepcSenderData_t *senderData, int64_t deltaTimeUs) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    if (deltaTimeUs < -3276000 || deltaTimeUs > 3276000)
        return PREPC_ERR_INVALID_ARGS;

    int64_t value;

    if (deltaTimeUs >= 0)
        value = (deltaTimeUs + 50) / 100;
    else
        value = (deltaTimeUs - 50) / 100;

    prepc_sender_set_int(senderData, PREPC_SENDER_FIELD_DELTA_TIME, &senderData->deltaTime, value,
                         2, true);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_fractional_frequency_8(prepcSenderData_t *senderData,
                                                          double fractionalFrequency) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    if (fractionalFrequency < 0.0 || fractionalFrequency >= 1.0)
        return PREPC_ERR_INVALID_ARGS;

    uint8_t fractionalFrequencyVal = (uint8_t)lround(fractionalFrequency * 256.0);

    prepc_sender_set_uint(senderData, PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY,
                          &senderData->fractionalFrequency, (uint64_t)fractionalFrequencyVal, 1,
                          true);

    return PREPC_ERR_OK;
}

prepcError_t prepc_sender_data_set_fractional_frequency_16(prepcSenderData_t *senderData,
                                                           double fractionalFrequency) {
    if (!senderData)
        return PREPC_ERR_INVALID_ARGS;

    if (fractionalFrequency < 0.0 || fractionalFrequency >= 1.0)
        return PREPC_ERR_INVALID_ARGS;

    uint16_t fractionalFrequencyVal = (uint16_t)lround(fractionalFrequency * 65536.0);

    prepc_sender_set_uint(senderData, PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY,
                          &senderData->fractionalFrequency, (uint64_t)fractionalFrequencyVal, 2,
                          true);

    return PREPC_ERR_OK;
}

void prepc_sender_data_reset(prepcSenderData_t *senderData) {
    if (!senderData)
        return;

    memset(senderData, 0, sizeof(*senderData));
}

prepcError_t prepc_templates_init(prepcTemplates_t *templates) {
    templates->receiverTemplateCount = 0;
    templates->senderTemplateCount = 0;
    templates->receiverTemplateMax = PREPC_MAX_RECEIVER_TEMPLATES;
    templates->senderTemplateMax = PREPC_MAX_SENDER_TEMPLATES;

    templates->nextTemplateId = PREPC_INITIAL_TEMPLATE_NUM;

    return PREPC_ERR_OK;
}

prepcError_t prepc_templates_add(prepcTemplates_t *templates, prepcSetType_t setType,
                                 const uint8_t *rfdKey, prepcTemplate_t **outTemplate) {
    if (!templates || !rfdKey || !outTemplate)
        return PREPC_ERR_INVALID_ARGS;

    prepcTemplate_t *templatesSet = NULL;
    size_t *templateCount = NULL;
    size_t templateMax = 0;

    if (setType == PREPC_SET_RECEIVER) {
        templatesSet = templates->receiverTemplate;
        templateCount = &templates->receiverTemplateCount;
        templateMax = templates->receiverTemplateMax;
    }
    else {
        templatesSet = templates->senderTemplate;
        templateCount = &templates->senderTemplateCount;
        templateMax = templates->senderTemplateMax;
    }

    if (templates->nextTemplateId >= PREPC_FINAL_TEMPLATE_NUM)
        return PREPC_ERR_TEMPLATES_FULL;

    if (*templateCount >= templateMax)
        return PREPC_ERR_TEMPLATES_FULL;

    prepcTemplate_t *writeTemplate = &templatesSet[*templateCount];

    binio_write_bytes(writeTemplate->key, rfdKey, PREPC_TEMPLATE_KEY_LEN);
    writeTemplate->templateId = templates->nextTemplateId++;
    writeTemplate->startupCount = 0;
    writeTemplate->lastSent = 0;

    (*templateCount)++;

    *outTemplate = writeTemplate;

    return PREPC_ERR_OK;
}

prepcError_t prepc_templates_find(prepcTemplates_t *templates, prepcSetType_t setType,
                                  const uint8_t *rfdKey, prepcTemplate_t **outTemplate) {
    if (!templates || !rfdKey || !outTemplate)
        return PREPC_ERR_INVALID_ARGS;

    prepcTemplate_t *templatesSet = NULL;
    size_t templateCount = 0;

    if (setType == PREPC_SET_RECEIVER) {
        templatesSet = templates->receiverTemplate;
        templateCount = templates->receiverTemplateCount;
    }
    else {
        templatesSet = templates->senderTemplate;
        templateCount = templates->senderTemplateCount;
    }

    for (size_t i = 0; i < templateCount; ++i) {
        if (memcmp(templatesSet[i].key, rfdKey, PREPC_TEMPLATE_KEY_LEN) == 0) {
            *outTemplate = &templatesSet[i];
            return PREPC_ERR_OK;
        }
    }

    *outTemplate = NULL;
    return PREPC_ERR_TEMPLATE_NOT_FOUND;
}

void prepc_templates_reset(prepcTemplates_t *templates) {
    if (!templates)
        return;

    templates->receiverTemplateCount = 0;
    templates->senderTemplateCount = 0;
    templates->nextTemplateId = PREPC_INITIAL_TEMPLATE_NUM;
}

void prepc_templates_soft_reset(prepcTemplates_t *templates) {
    if (!templates)
        return;

    for (size_t i = 0; i < templates->receiverTemplateCount; ++i) {
        templates->receiverTemplate[i].startupCount = 0;
        templates->receiverTemplate[i].lastSent = 0;
    }

    for (size_t i = 0; i < templates->senderTemplateCount; ++i) {
        templates->senderTemplate[i].startupCount = 0;
        templates->senderTemplate[i].lastSent = 0;
    }
}

prepcError_t prepc_ctx_init(prepcCtx_t *ctx, const char *host, const char *port) {
    ctx->sequenceNum = 0;
    if (!hal_system_random_u32(&ctx->sessionId))
        return PREPC_ERR_SYSTEM;
    ctx->activeSenderTemplate = NULL;
    ctx->activeReceiverTemplate = NULL;
    ctx->currentReceiverData = NULL;
    ctx->receiverRfdBuffered = false;
    ctx->senderRfdBuffered = false;
    ctx->lastDNSSync = 0;
    ctx->lastPacketSentTime = 0;

    halUdpErr_t udpErr = hal_udp_init(&ctx->udpCtx, host, port);
    if (udpErr != HAL_UDP_ERR_OK)
        return PREPC_ERR_NETWORK;

    prepcError_t rc;

    rc = prepc_buf_init(&ctx->buf);
    if (rc != PREPC_ERR_OK)
        return rc;

    rc = prepc_templates_init(&ctx->templates);
    if (rc != PREPC_ERR_OK) {
        return rc;
    }

    return PREPC_ERR_OK;
}

void prepc_ctx_free(prepcCtx_t *ctx) {
    ctx->sequenceNum = 0;
    ctx->sessionId = 0;
    ctx->activeSenderTemplate = NULL;
    ctx->activeReceiverTemplate = NULL;
    ctx->currentReceiverData = NULL;
    ctx->receiverRfdBuffered = false;
    ctx->senderRfdBuffered = false;

    hal_udp_cleanup(ctx->udpCtx);
}

prepcError_t prepc_ctx_reset(prepcCtx_t *ctx) {
    if (!ctx)
        return PREPC_ERR_INVALID_ARGS;

    if (!hal_system_random_u32(&ctx->sessionId))
        return PREPC_ERR_SYSTEM;

    ctx->sequenceNum = 0;
    ctx->lastPacketSentTime = 0;
    ctx->activeSenderTemplate = NULL;
    ctx->activeReceiverTemplate = NULL;
    ctx->receiverRfdBuffered = false;
    ctx->senderRfdBuffered = false;

    prepc_buf_reset(&ctx->buf);
    prepc_templates_reset(&ctx->templates);

    return PREPC_ERR_OK;
}

prepcError_t prepc_ctx_send_manually(prepcCtx_t *ctx) {
    if (!ctx)
        return PREPC_ERR_INVALID_ARGS;

    if (ctx->buf.len == 0)
        return PREPC_ERR_OK;

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PREPC_ERR_SYSTEM;
    uint32_t randomId;
    if (!hal_system_random_u32(&randomId))
        return PREPC_ERR_SYSTEM;

    prepcError_t rc;
    rc = prepc_write_packet_header(&ctx->buf, ctx->buf.len, (uint32_t)currentTime, ctx->sequenceNum,
                                   ctx->sessionId);
    if (rc != PREPC_ERR_OK)
        return rc;

    if (hal_udp_send(ctx->udpCtx, ctx->buf.data, ctx->buf.len) != HAL_UDP_ERR_OK)
        return PREPC_ERR_NETWORK;
    DEBUG_printf("Buffer sent, %zu bytes", ctx->buf.len);

    ctx->sequenceNum++;
    ctx->lastPacketSentTime = currentTime;

    if (ctx->receiverRfdBuffered && ctx->activeReceiverTemplate) {
        ctx->activeReceiverTemplate->startupCount++;
        ctx->activeReceiverTemplate->lastSent = currentTime;
        DEBUG_printf("Receiver RFD in the buffer, incrementing startupCount");
    }
    ctx->receiverRfdBuffered = false;
    ctx->activeReceiverTemplate = NULL;

    if (ctx->senderRfdBuffered && ctx->activeSenderTemplate) {
        ctx->activeSenderTemplate->startupCount++;
        ctx->activeSenderTemplate->lastSent = currentTime;
        DEBUG_printf("Sender RFD in the buffer, incrementing startupCount");
    }
    ctx->senderRfdBuffered = false;
    ctx->activeSenderTemplate = NULL;

    prepc_buf_reset(&ctx->buf);

    if (ctx->sequenceNum == UINT32_MAX) {
        ctx->sequenceNum = 0;
        ctx->lastPacketSentTime = 0;
        ctx->sessionId = randomId;
        prepc_templates_soft_reset(&ctx->templates);
        DEBUG_printf("SequenceNum overflow detected, reseting context");
    }

    if ((currentTime - ctx->lastDNSSync) >= PREPC_DNS_TTL_SEC) {

        halUdpErr_t udpErr = hal_udp_reresolve(ctx->udpCtx);

        if (udpErr != HAL_UDP_ERR_OK && udpErr != HAL_UDP_ERR_OK_HOST_CHANGED)
            return PREPC_ERR_NETWORK;

        ctx->lastDNSSync = currentTime;

        if (udpErr == HAL_UDP_ERR_OK_HOST_CHANGED) {
            // Host has changed
            ctx->sequenceNum = 0;
            ctx->lastPacketSentTime = 0;
            ctx->sessionId = randomId;
            prepc_templates_soft_reset(&ctx->templates);
            DEBUG_printf("Host IP changed, reseting context");
        }
    }

    return PREPC_ERR_OK;
}

prepcError_t prepc_ctx_set_receiver(prepcCtx_t *ctx, const prepcReceiverData_t *receiverData) {
    if (!ctx || !receiverData)
        return PREPC_ERR_INVALID_ARGS;

    if ((receiverData->fields & PREPC_RECEIVER_MANDATORY_FIELDS_MASK) !=
        PREPC_RECEIVER_MANDATORY_FIELDS_MASK)
        return PREPC_ERR_MISSING_FIELDS;

    prepcError_t rc;

    if (ctx->activeSenderTemplate) {
        DEBUG_printf("activeSenderTemplate in the buffer, sending it first");
        rc = prepc_ctx_send_manually(ctx);
        if (rc != PREPC_ERR_OK)
            return rc;
    }
    else {
        prepc_buf_reset(&ctx->buf);
    }

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PREPC_ERR_SYSTEM;

    rc = prepc_write_packet_header(&ctx->buf, 0, 0, 0, 0);
    if (rc != PREPC_ERR_OK) {
        prepc_buf_reset(&ctx->buf);
        return rc;
    }

    uint8_t rfdKey[PREPC_TEMPLATE_KEY_LEN];
    rc = prepc_generate_receiver_rfd_key(receiverData, rfdKey);
    if (rc != PREPC_ERR_OK)
        return rc;

    prepcTemplate_t *foundTemplate;

    rc = prepc_templates_find(&ctx->templates, PREPC_SET_RECEIVER, rfdKey, &foundTemplate);
    if (rc != PREPC_ERR_OK && rc != PREPC_ERR_TEMPLATE_NOT_FOUND)
        return rc;

    if (rc == PREPC_ERR_TEMPLATE_NOT_FOUND) {
        rc = prepc_templates_add(&ctx->templates, PREPC_SET_RECEIVER, rfdKey, &foundTemplate);
        if (rc != PREPC_ERR_OK && rc != PREPC_ERR_TEMPLATES_FULL)
            return rc;
        DEBUG_printf("template not found, trying to add it");

        if (rc == PREPC_ERR_TEMPLATES_FULL) {
            DEBUG_printf("templates cache full, trying to reset the context");
            rc = prepc_ctx_reset(ctx);
            if (rc != PREPC_ERR_OK)
                return rc;

            return prepc_ctx_set_receiver(ctx, receiverData);
        }
    }

    if (foundTemplate->startupCount <= (PREPC_RFD_NUM_STARTUP - 1) ||
        (currentTime - foundTemplate->lastSent) >= PREPC_RFD_SEC_TIMEOUT) {

        rc = prepc_append_receiver_rfd(&ctx->buf, receiverData, foundTemplate->templateId);

        if (rc != PREPC_ERR_OK) {
            prepc_buf_reset(&ctx->buf);
            return rc;
        }
        ctx->receiverRfdBuffered = true;
    }

    rc = prepc_append_receiver_data(&ctx->buf, receiverData, foundTemplate->templateId);
    if (rc != PREPC_ERR_OK) {
        prepc_buf_reset(&ctx->buf);
        return rc;
    }

    ctx->activeReceiverTemplate = foundTemplate;
    ctx->currentReceiverData = receiverData;

    return PREPC_ERR_OK;
}

prepcError_t prepc_ctx_add_sender(prepcCtx_t *ctx, const prepcSenderData_t *senderData) {
    if (!ctx || !senderData)
        return PREPC_ERR_INVALID_ARGS;

    if ((senderData->fields & PREPC_SENDER_MANDATORY_FIELDS_MASK) !=
        PREPC_SENDER_MANDATORY_FIELDS_MASK)
        return PREPC_ERR_MISSING_FIELDS;

    if (!ctx->currentReceiverData)
        return PREPC_ERR_INVALID_STATE;

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PREPC_ERR_SYSTEM;

    prepcError_t rc;

    uint8_t rfdKey[PREPC_TEMPLATE_KEY_LEN];
    rc = prepc_generate_sender_rfd_key(senderData, rfdKey);
    if (rc != PREPC_ERR_OK)
        return rc;

    prepcTemplate_t *foundTemplate;

    rc = prepc_templates_find(&ctx->templates, PREPC_SET_SENDER, rfdKey, &foundTemplate);
    if (rc != PREPC_ERR_OK && rc != PREPC_ERR_TEMPLATE_NOT_FOUND)
        return rc;

    if (rc == PREPC_ERR_TEMPLATE_NOT_FOUND) {
        DEBUG_printf("template not found, trying to add it");
        rc = prepc_templates_add(&ctx->templates, PREPC_SET_SENDER, rfdKey, &foundTemplate);
        if (rc != PREPC_ERR_OK && rc != PREPC_ERR_TEMPLATES_FULL)
            return rc;

        if (rc == PREPC_ERR_TEMPLATES_FULL) {
            DEBUG_printf("templates cache full, trying to reset the context");
            if (ctx->activeSenderTemplate) {
                rc = prepc_ctx_send_manually(ctx);
                if (rc != PREPC_ERR_OK)
                    return rc;
            }

            rc = prepc_ctx_reset(ctx);
            if (rc != PREPC_ERR_OK)
                return rc;

            rc = prepc_ctx_set_receiver(ctx, ctx->currentReceiverData);
            if (rc != PREPC_ERR_OK)
                return rc;

            return prepc_ctx_add_sender(ctx, senderData);
        }
    }

    if (ctx->activeSenderTemplate && ctx->activeSenderTemplate == foundTemplate) {
        rc = prepc_append_sender_data(&ctx->buf, senderData, foundTemplate->templateId);
        if (rc != PREPC_ERR_BUF_TOO_SMALL)
            return rc;
    }

    DEBUG_printf("new senderData doesn't fit, sending the buffer");
    rc = prepc_ctx_set_receiver(ctx, ctx->currentReceiverData);
    if (rc != PREPC_ERR_OK)
        return rc;

    size_t rollbackLen = ctx->buf.len;
    if (foundTemplate->startupCount <= (PREPC_RFD_NUM_STARTUP - 1) ||
        (currentTime - foundTemplate->lastSent) >= PREPC_RFD_SEC_TIMEOUT) {

        rc = prepc_append_sender_rfd(&ctx->buf, senderData, foundTemplate->templateId);
        if (rc != PREPC_ERR_OK)
            return rc;

        ctx->senderRfdBuffered = true;
    }

    DEBUG_printf("adding senderData to the new buffer");
    rc = prepc_append_sender_data(&ctx->buf, senderData, foundTemplate->templateId);
    if (rc != PREPC_ERR_OK) {
        ctx->buf.len = rollbackLen;
        return rc;
    }

    ctx->activeSenderTemplate = foundTemplate;

    return PREPC_ERR_OK;
}

prepcError_t prepc_ctx_flush(prepcCtx_t *ctx, uint64_t minIntervalSecs,
                             bool allowReceiverDataOnly) {
    if (!ctx)
        return PREPC_ERR_INVALID_ARGS;

    if (!ctx->currentReceiverData)
        return PREPC_ERR_INVALID_STATE;

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PREPC_ERR_SYSTEM;

    if ((currentTime - ctx->lastPacketSentTime) >= minIntervalSecs) {
        if (allowReceiverDataOnly) {
            prepcError_t rc;

            rc = prepc_ctx_send_manually(ctx);

            if (rc != PREPC_ERR_OK)
                return rc;
        }
        return prepc_ctx_set_receiver(ctx, ctx->currentReceiverData);
    }
    else {
        return PREPC_ERR_OK;
    }
}
