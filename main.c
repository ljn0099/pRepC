#include "binio.h"
#include "modes.h"
#include "pskLibC.h"
#include "systemHal.h"
#include "time.h"
#include "udpHal.h"
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PSK_MAX_STR_LEN 254

#define PSK_IPFIX_VERSION 0x000A

#define PSK_RECEIVER_DATA_FLAG 0x0003
#define PSK_RECEIVER_SCOPE_FIELDS_COUNT 0x0001

#define PSK_SENDER_DATA_FLAG 0x0002

#define PSK_ENTERPRISE_NUM 0x0000768F

#define PSK_VARIABLE_STR 0xFFFF

#define PSK_ENTERPRISE_MASK 0x8000

#define PSK_PADDING_DIVISOR 4

#define PSK_DATA_HEADER_LEN 2

typedef uint32_t pskFields_t;

#define PSK_FIELD_MASK(field) ((pskFields_t)1u << (field))

#define PSK_MAX_SENDER_TEMPLATES 15
#define PSK_MAX_RECEIVER_TEMPLATES 15

#define PSK_INITIAL_TEMPLATE_NUM 256
#define PSK_FINAL_TEMPLATE_NUM 65535

#define PSK_PACKET_LEN 1400

#define PSK_RFD_NUM_STARTUP 5
#define PSK_RFD_SEC_TIMEOUT 2700 // 45 minutes

typedef enum {
    PSK_RECEIVER_FIELD_CALLSIGN = 0,
    PSK_RECEIVER_FIELD_LOCATOR,
    PSK_RECEIVER_FIELD_DECODER_SOFTWARE,
    PSK_RECEIVER_FIELD_ANTENNA_INFO,
    PSK_RECEIVER_FIELD_PERSISTENT_ID,
    PSK_RECEIVER_FIELD_RIG_INFO,

    PSK_RECEIVER_FIELD_COUNT
} pskReceiverField_t;

typedef enum {
    PSK_SENDER_FIELD_CALLSIGN = 0,
    PSK_SENDER_FIELD_LOCATOR,
    PSK_SENDER_FIELD_FREQUENCY,
    PSK_SENDER_FIELD_SNR,
    PSK_SENDER_FIELD_IMD,
    PSK_SENDER_FIELD_MODE,
    PSK_SENDER_FIELD_INFO_SRC,
    PSK_SENDER_FIELD_FLOW_START_SECS,
    PSK_SENDER_FIELD_MESSAGE_BITS,
    PSK_SENDER_FIELD_DELTA_TIME,
    PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY,

    PSK_SENDER_FIELD_COUNT
} pskSenderField_t;

#define PSK_RECEIVER_MANDATORY_FIELDS_MASK                                                         \
    (PSK_FIELD_MASK(PSK_RECEIVER_FIELD_CALLSIGN) | PSK_FIELD_MASK(PSK_RECEIVER_FIELD_LOCATOR) |    \
     PSK_FIELD_MASK(PSK_RECEIVER_FIELD_DECODER_SOFTWARE))

#define PSK_SENDER_MANDATORY_FIELDS_MASK                                                           \
    (PSK_FIELD_MASK(PSK_SENDER_FIELD_CALLSIGN) | PSK_FIELD_MASK(PSK_SENDER_FIELD_MODE) |           \
     PSK_FIELD_MASK(PSK_SENDER_FIELD_INFO_SRC) | PSK_FIELD_MASK(PSK_SENDER_FIELD_FLOW_START_SECS))

#define PSK_TEMPLATE_KEY_LEN                                                                       \
    (sizeof(pskFields_t) + (((size_t)PSK_SENDER_FIELD_COUNT > (size_t)PSK_RECEIVER_FIELD_COUNT)                    \
                                ? (size_t)PSK_SENDER_FIELD_COUNT                                           \
                                : (size_t)PSK_RECEIVER_FIELD_COUNT))

static const uint16_t receiverFieldIds[PSK_RECEIVER_FIELD_COUNT] = {
    [PSK_RECEIVER_FIELD_CALLSIGN] = 0x8002,         [PSK_RECEIVER_FIELD_LOCATOR] = 0x8004,
    [PSK_RECEIVER_FIELD_DECODER_SOFTWARE] = 0x8008, [PSK_RECEIVER_FIELD_ANTENNA_INFO] = 0x8009,
    [PSK_RECEIVER_FIELD_PERSISTENT_ID] = 0x800C,    [PSK_RECEIVER_FIELD_RIG_INFO] = 0x800D,
};

static const uint16_t senderFieldIds[PSK_SENDER_FIELD_COUNT] = {
    [PSK_SENDER_FIELD_CALLSIGN] = 0x8001,
    [PSK_SENDER_FIELD_LOCATOR] = 0x8003,
    [PSK_SENDER_FIELD_FREQUENCY] = 0x8005,
    [PSK_SENDER_FIELD_SNR] = 0x8006,
    [PSK_SENDER_FIELD_IMD] = 0x8007,
    [PSK_SENDER_FIELD_MODE] = 0x800A,
    [PSK_SENDER_FIELD_INFO_SRC] = 0x800B,
    [PSK_SENDER_FIELD_FLOW_START_SECS] = 0x0096,
    [PSK_SENDER_FIELD_MESSAGE_BITS] = 0x800E,
    [PSK_SENDER_FIELD_DELTA_TIME] = 0x800F,
    [PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY] = 0x8010,
};

typedef enum {
    PSK_INFO_SRC_AUTO = 1,
    PSK_INFO_SRC_CALL_LOG = 2,
    PSK_INFO_SRC_MANUAL = 3,
} pskInfoSrc_t;

#define PSK_INFO_SRC_TEST_MASK 0x80

typedef struct {
    pskFields_t fields;
    uint8_t lengths[PSK_RECEIVER_FIELD_COUNT];

    const char *callsign;
    const char *locator;
    const char *decoderSoftware;
    const char *antennaInfo;
    const char *persistentId;
    const char *rigInfo;
} pskReceiverData_t;

typedef struct {
    pskFields_t fields;
    uint8_t lengths[PSK_SENDER_FIELD_COUNT];

    const char *callsign;
    const char *locator;
    uint64_t frequency;
    int64_t snr;
    int64_t imd;
    const char *mode;
    uint64_t infoSrc;
    uint64_t flowStartSecs;
    const uint8_t *messageBits;
    int64_t deltaTime;
    uint64_t fractionalFrequency;
} pskSenderData_t;

typedef struct {
    uint8_t key[PSK_TEMPLATE_KEY_LEN];
    uint16_t templateId;
    uint8_t startupCount;
    uint64_t lastSent;
} pskTemplate_t;

typedef struct {
    pskTemplate_t receiverTemplate[PSK_MAX_RECEIVER_TEMPLATES];
    size_t receiverTemplateCount;
    size_t receiverTemplateMax;

    pskTemplate_t senderTemplate[PSK_MAX_SENDER_TEMPLATES];
    size_t senderTemplateCount;
    size_t senderTemplateMax;

    uint16_t nextTemplateId;
} pskTemplates_t;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t maxLen;
} pskBuf_t;

typedef struct {
    uint32_t sequenceNum;
    uint32_t sessionId;

    pskTemplate_t *activeReceiverTemplate;
    pskTemplate_t *activeSenderTemplate;
    bool receiverRfdBuffered;
    bool senderRfdBuffered;

    pskBuf_t buf;
    pskTemplates_t templates;

    const pskReceiverData_t *currentReceiverData;

    void *udpCtx;
} pskCtx_t;

typedef enum { PSK_SET_SENDER = 0, PSK_SET_RECEIVER } pskSetType_t;

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

static inline size_t psk_popcount(pskFields_t value) {
    size_t count = 0;

    while (value) {
        value &= value - 1;
        ++count;
    }

    return count;
}

static inline bool psk_has_space(const pskBuf_t *buf, size_t bytes) {
    return (buf->maxLen - buf->len >= bytes);
}

static inline size_t psk_append_padding(pskBuf_t *buf, size_t written) {
    size_t paddingBytes =
        (PSK_PADDING_DIVISOR - (written % PSK_PADDING_DIVISOR)) % PSK_PADDING_DIVISOR;

    if (buf)
        buf->len += binio_skip_zero(buf->data + buf->len, paddingBytes);

    return paddingBytes;
}

static inline size_t psk_append_u8(pskBuf_t *buf, uint8_t value) {
    if (buf)
        buf->len += binio_write_u8(buf->data + buf->len, value);

    return sizeof(uint8_t);
}

static inline size_t psk_append_u16(pskBuf_t *buf, uint16_t value) {
    if (buf)
        buf->len += binio_write_u16_be(buf->data + buf->len, value);

    return sizeof(uint16_t);
}

static inline size_t psk_append_u32(pskBuf_t *buf, uint32_t value) {
    if (buf)
        buf->len += binio_write_u32_be(buf->data + buf->len, value);

    return sizeof(uint32_t);
}

static inline size_t psk_append_u64(pskBuf_t *buf, uint64_t value) {
    if (buf)
        buf->len += binio_write_u64_be(buf->data + buf->len, value);

    return sizeof(uint64_t);
}

static inline size_t psk_append_var_uint(pskBuf_t *buf, uint64_t value, size_t nBytes) {
    if (nBytes > sizeof(uint64_t))
        nBytes = sizeof(uint64_t);

    if (buf)
        buf->len += binio_write_u64_lsb_be(buf->data + buf->len, value, nBytes);

    return nBytes;
}

static inline size_t psk_append_var_int(pskBuf_t *buf, int64_t value, size_t nBytes) {
    if (nBytes > sizeof(int64_t))
        nBytes = sizeof(int64_t);

    if (buf)
        buf->len += binio_write_i64_lsb_be(buf->data + buf->len, value, nBytes);

    return nBytes;
}

static inline size_t psk_uint_min_bytes(uint64_t value) {
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

static inline size_t psk_int_min_bytes(int64_t value) {
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

static inline size_t psk_append_bytes(pskBuf_t *buf, const uint8_t *bytes, size_t nBytes) {
    if (buf)
        buf->len += binio_write_bytes(buf->data + buf->len, bytes, nBytes);

    return nBytes;
}

static inline size_t psk_append_str(pskBuf_t *buf, const char *str, size_t strLen) {
    if (strLen >= PSK_MAX_STR_LEN)
        strLen = PSK_MAX_STR_LEN;

    if (buf) {
        buf->len += binio_write_u8(buf->data + buf->len, (uint8_t)strLen);
        buf->len += binio_write_bytes(buf->data + buf->len, (const uint8_t *)str, strLen);
    }

    return sizeof(uint8_t) + strLen;
}

static size_t psk_encode_receiver_data(pskBuf_t *buf, const pskReceiverData_t *receiverData,
                                       uint16_t templateId, size_t receiverDataLen) {
    size_t written = 0;

    // 0. Header
    written += psk_append_u16(buf, (uint16_t)templateId);
    written += psk_append_u16(buf, (uint16_t)receiverDataLen);

    // 1. Callsign
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_CALLSIGN))
        written += psk_append_str(buf, receiverData->callsign,
                                  receiverData->lengths[PSK_RECEIVER_FIELD_CALLSIGN]);

    // 2. Locator
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_LOCATOR))
        written += psk_append_str(buf, receiverData->locator,
                                  receiverData->lengths[PSK_RECEIVER_FIELD_LOCATOR]);

    // 3. Decoder Software
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_DECODER_SOFTWARE))
        written += psk_append_str(buf, receiverData->decoderSoftware,
                                  receiverData->lengths[PSK_RECEIVER_FIELD_DECODER_SOFTWARE]);

    // 4. Antenna Information
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_ANTENNA_INFO))
        written += psk_append_str(buf, receiverData->antennaInfo,
                                  receiverData->lengths[PSK_RECEIVER_FIELD_ANTENNA_INFO]);

    // 5. Persistend Identifier
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_PERSISTENT_ID))
        written += psk_append_str(buf, receiverData->persistentId,
                                  receiverData->lengths[PSK_RECEIVER_FIELD_PERSISTENT_ID]);

    // 6. Rig Information
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_RIG_INFO))
        written += psk_append_str(buf, receiverData->rigInfo,
                                  receiverData->lengths[PSK_RECEIVER_FIELD_RIG_INFO]);

    written += psk_append_padding(buf, written);

    return written;
}

static size_t psk_encode_sender_data(pskBuf_t *buf, const pskSenderData_t *senderData,
                                     uint16_t templateId, size_t senderDataLen) {
    size_t written = 0;

    // 0. Header
    written += psk_append_u16(buf, (uint16_t)templateId);
    written += psk_append_u16(buf, (uint16_t)senderDataLen);

    // 1. Callsign
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_CALLSIGN))
        written += psk_append_str(buf, senderData->callsign,
                                  senderData->lengths[PSK_SENDER_FIELD_CALLSIGN]);

    // 2. Locator
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_LOCATOR))
        written +=
            psk_append_str(buf, senderData->locator, senderData->lengths[PSK_SENDER_FIELD_LOCATOR]);

    // 3. Frequency
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_FREQUENCY))
        written += psk_append_var_uint(buf, senderData->frequency,
                                       senderData->lengths[PSK_SENDER_FIELD_FREQUENCY]);

    // 4. SNR
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_SNR))
        written +=
            psk_append_var_int(buf, senderData->snr, senderData->lengths[PSK_SENDER_FIELD_SNR]);

    // 5. IMD
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_IMD))
        written +=
            psk_append_var_int(buf, senderData->imd, senderData->lengths[PSK_SENDER_FIELD_IMD]);

    // 6. Mode
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_MODE))
        written +=
            psk_append_str(buf, senderData->mode, senderData->lengths[PSK_SENDER_FIELD_MODE]);

    // 7. Information Source
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_INFO_SRC))
        written += psk_append_var_uint(buf, senderData->infoSrc,
                                       senderData->lengths[PSK_SENDER_FIELD_INFO_SRC]);

    // 8. Flow Starts Seconds
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_FLOW_START_SECS))
        written += psk_append_var_uint(buf, senderData->flowStartSecs,
                                       senderData->lengths[PSK_SENDER_FIELD_FLOW_START_SECS]);

    // 9. Message Bits
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_MESSAGE_BITS))
        written += psk_append_bytes(buf, senderData->messageBits,
                                    senderData->lengths[PSK_SENDER_FIELD_MESSAGE_BITS]);

    // 10. Delta Time
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_DELTA_TIME))
        written += psk_append_var_int(buf, senderData->deltaTime,
                                      senderData->lengths[PSK_SENDER_FIELD_DELTA_TIME]);

    // 11. Fractional Frequency
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY))
        written += psk_append_var_uint(buf, senderData->fractionalFrequency,
                                       senderData->lengths[PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY]);

    written += psk_append_padding(buf, written);

    return written;
}

bool psk_append_receiver_data(pskBuf_t *buf, const pskReceiverData_t *receiverData,
                              uint16_t templateId) {
    if (!buf || !receiverData)
        return PSK_ERR_INVALID_ARGS;

    size_t receiverDataLen = psk_encode_receiver_data(NULL, receiverData, templateId, 0);

    if (!psk_has_space(buf, receiverDataLen))
        return PSK_ERR_BUF_TOO_SMALL;

    psk_encode_receiver_data(buf, receiverData, templateId, receiverDataLen);

    return PSK_ERR_OK;
}

bool psk_append_sender_data(pskBuf_t *buf, const pskSenderData_t *senderData, uint16_t templateId) {
    if (!buf || !senderData)
        return PSK_ERR_INVALID_ARGS;

    size_t senderDataLen = psk_encode_sender_data(NULL, senderData, templateId, 0);

    if (!psk_has_space(buf, senderDataLen))
        return PSK_ERR_BUF_TOO_SMALL;

    psk_encode_sender_data(buf, senderData, templateId, senderDataLen);

    return PSK_ERR_OK;
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

static inline size_t psk_append_field_rfd(pskBuf_t *buf, const uint16_t *fieldIds, unsigned field,
                                          uint16_t len) {
    uint16_t id = fieldIds[field];

    size_t written = 0;

    written += psk_append_u16(buf, id);
    written += psk_append_u16(buf, len);

    if (id & PSK_ENTERPRISE_MASK)
        written += psk_append_u32(buf, PSK_ENTERPRISE_NUM);

    return written;
}

static size_t psk_encode_receiver_rfd(pskBuf_t *buf, const pskReceiverData_t *receiverData,
                                      uint16_t templateId, size_t receiverRfdLen, uint8_t *rfdKey) {
    uint8_t *keyP = NULL;
    if (rfdKey) {
        keyP = rfdKey;
        binio_skip_zero(keyP, PSK_TEMPLATE_KEY_LEN);

        keyP +=
            binio_write_bytes(keyP, (uint8_t *)&receiverData->fields, sizeof(receiverData->fields));
    }

    size_t written = 0;

    // 0. Header
    written += psk_append_u16(buf, (uint16_t)PSK_RECEIVER_DATA_FLAG);
    written += psk_append_u16(buf, (uint16_t)receiverRfdLen);
    written += psk_append_u16(buf, templateId);
    written += psk_append_u16(buf, (uint16_t)psk_popcount(receiverData->fields));
    written += psk_append_u16(buf, (uint16_t)PSK_RECEIVER_SCOPE_FIELDS_COUNT);

    // 1. Callsign
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_CALLSIGN)) {
        written += psk_append_field_rfd(buf, receiverFieldIds, PSK_RECEIVER_FIELD_CALLSIGN,
                                        PSK_VARIABLE_STR);
    }

    // 2. Locator
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_LOCATOR)) {
        written += psk_append_field_rfd(buf, receiverFieldIds, PSK_RECEIVER_FIELD_LOCATOR,
                                        PSK_VARIABLE_STR);
    }

    // 3. Decoder Software
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_DECODER_SOFTWARE)) {
        written += psk_append_field_rfd(buf, receiverFieldIds, PSK_RECEIVER_FIELD_DECODER_SOFTWARE,
                                        PSK_VARIABLE_STR);
    }

    // 4. Antenna Info
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_ANTENNA_INFO)) {
        written += psk_append_field_rfd(buf, receiverFieldIds, PSK_RECEIVER_FIELD_ANTENNA_INFO,
                                        PSK_VARIABLE_STR);
    }

    // 5. Persistent Identifier
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_PERSISTENT_ID)) {
        written += psk_append_field_rfd(buf, receiverFieldIds, PSK_RECEIVER_FIELD_PERSISTENT_ID,
                                        PSK_VARIABLE_STR);
    }

    // 6. Persistent Identifier
    if (receiverData->fields & PSK_FIELD_MASK(PSK_RECEIVER_FIELD_RIG_INFO)) {
        written += psk_append_field_rfd(buf, receiverFieldIds, PSK_RECEIVER_FIELD_RIG_INFO,
                                        PSK_VARIABLE_STR);
    }

    written += psk_append_padding(buf, written);

    return written;
}

static size_t psk_encode_sender_rfd(pskBuf_t *buf, const pskSenderData_t *senderData,
                                    uint16_t templateId, size_t senderRfdLen, uint8_t *rfdKey) {
    uint8_t *keyP = NULL;
    if (rfdKey) {
        keyP = rfdKey;
        binio_skip_zero(keyP, PSK_TEMPLATE_KEY_LEN);

        keyP += binio_write_bytes(keyP, (uint8_t *)&senderData->fields, sizeof(senderData->fields));
    }

    size_t written = 0;

    // 0. Header
    written += psk_append_u16(buf, (uint16_t)PSK_SENDER_DATA_FLAG);
    written += psk_append_u16(buf, (uint16_t)senderRfdLen);
    written += psk_append_u16(buf, templateId);
    written += psk_append_u16(buf, (uint16_t)psk_popcount(senderData->fields));

    // 1. Callsign
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_CALLSIGN)) {
        written +=
            psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_CALLSIGN, PSK_VARIABLE_STR);
    }

    // 2. Locator
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_LOCATOR)) {
        written +=
            psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_LOCATOR, PSK_VARIABLE_STR);
    }

    // 3. Frequency
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_FREQUENCY)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_FREQUENCY,
                                        senderData->lengths[PSK_SENDER_FIELD_FREQUENCY]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_FREQUENCY]);
    }

    // 4. SNR
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_SNR)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_SNR,
                                        senderData->lengths[PSK_SENDER_FIELD_SNR]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_SNR]);
    }

    // 5. IMD
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_IMD)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_IMD,
                                        senderData->lengths[PSK_SENDER_FIELD_IMD]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_IMD]);
    }

    // 6. Mode
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_MODE)) {
        written +=
            psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_MODE, PSK_VARIABLE_STR);
    }

    // 7. Information Source
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_INFO_SRC)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_INFO_SRC,
                                        senderData->lengths[PSK_SENDER_FIELD_INFO_SRC]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_INFO_SRC]);
    }

    // 8. Flow Start Seconds
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_FLOW_START_SECS)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_FLOW_START_SECS,
                                        senderData->lengths[PSK_SENDER_FIELD_FLOW_START_SECS]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_FLOW_START_SECS]);
    }

    // 9. Message Bits
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_MESSAGE_BITS)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_MESSAGE_BITS,
                                        senderData->lengths[PSK_SENDER_FIELD_MESSAGE_BITS]);
    }

    // 10. Delta Time
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_DELTA_TIME)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_DELTA_TIME,
                                        senderData->lengths[PSK_SENDER_FIELD_DELTA_TIME]);

        if (keyP)
            keyP += binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_DELTA_TIME]);
    }

    // 11. Fractional Frequency
    if (senderData->fields & PSK_FIELD_MASK(PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY)) {
        written += psk_append_field_rfd(buf, senderFieldIds, PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY,
                                        senderData->lengths[PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY]);

        if (keyP)
            keyP +=
                binio_write_u8(keyP, senderData->lengths[PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY]);
    }

    written += psk_append_padding(buf, written);

    return written;
}

pskError_t psk_append_receiver_rfd(pskBuf_t *buf, const pskReceiverData_t *receiverData,
                                   uint16_t templateId) {
    if (!buf || !receiverData)
        return PSK_ERR_INVALID_ARGS;

    size_t receiverRfdLen = psk_encode_receiver_rfd(NULL, receiverData, templateId, 0, NULL);

    if (!psk_has_space(buf, receiverRfdLen))
        return PSK_ERR_BUF_TOO_SMALL;

    psk_encode_receiver_rfd(buf, receiverData, templateId, receiverRfdLen, NULL);

    return PSK_ERR_OK;
}

pskError_t psk_append_sender_rfd(pskBuf_t *buf, const pskSenderData_t *senderData,
                                 uint16_t templateId) {
    if (!buf || !senderData)
        return PSK_ERR_INVALID_ARGS;

    size_t senderRfdLen = psk_encode_sender_rfd(NULL, senderData, templateId, 0, NULL);

    if (!psk_has_space(buf, senderRfdLen))
        return PSK_ERR_BUF_TOO_SMALL;

    psk_encode_sender_rfd(buf, senderData, templateId, senderRfdLen, NULL);

    return PSK_ERR_OK;
}

pskError_t psk_generate_receiver_rfd_key(const pskReceiverData_t *receiverData, uint8_t *rfdKey) {
    if (!receiverData || !rfdKey)
        return PSK_ERR_INVALID_ARGS;

    psk_encode_receiver_rfd(NULL, receiverData, 0, 0, rfdKey);

    return PSK_ERR_OK;
}

pskError_t psk_generate_sender_rfd_key(const pskSenderData_t *senderData, uint8_t *rfdKey) {
    if (!senderData || !rfdKey)
        return PSK_ERR_INVALID_ARGS;

    psk_encode_sender_rfd(NULL, senderData, 0, 0, rfdKey);

    return PSK_ERR_OK;
}

pskError_t psk_write_packet_header(pskBuf_t *buf, size_t datagramLen, uint32_t txTimestamp,
                                   uint32_t sequenceNum, uint32_t sessionId) {
    // Header Format (16 bytes total)
    // <IpfixVersion:2><DatagramLen:2><TxTimestamp:4><SequenceNum:4><SessionId:4>

    if (buf->maxLen < 16)
        return PSK_ERR_BUF_TOO_SMALL;

    if (buf->len < 16)
        buf->len = 16;

    uint8_t *p = buf->data;

    p += binio_write_u16_be(p, (uint16_t)PSK_IPFIX_VERSION);
    p += binio_write_u16_be(p, (uint16_t)datagramLen);
    p += binio_write_u32_be(p, txTimestamp);
    p += binio_write_u32_be(p, sequenceNum);
    p += binio_write_u32_be(p, sessionId);

    return PSK_ERR_OK;
}

pskError_t psk_buf_init(pskBuf_t *buf, size_t bufLen) {
    buf->data = malloc(bufLen);
    if (!buf->data)
        return PSK_ERR_MEMORY;

    buf->len = 0;
    buf->maxLen = bufLen;

    return PSK_ERR_OK;
}

void psk_buf_reset(pskBuf_t *buf) {
    buf->len = 0;
}

void psk_buf_free(pskBuf_t *buf) {
    free(buf->data);
    buf->len = 0;
    buf->maxLen = 0;
}

static inline void psk_set_str(uint32_t *fields, uint8_t *lengths, unsigned field, const char **dst,
                               const char *value, size_t len) {
    *fields |= PSK_FIELD_MASK(field);
    *dst = value;
    lengths[field] = (uint8_t)len;
}

static inline void psk_receiver_set_str(pskReceiverData_t *receiverData, pskReceiverField_t field,
                                        const char **dst, const char *value, size_t len) {
    psk_set_str(&receiverData->fields, receiverData->lengths, field, dst, value, len);
}

static inline void psk_sender_set_str(pskSenderData_t *senderData, pskSenderField_t field,
                                      const char **dst, const char *value, size_t len) {
    psk_set_str(&senderData->fields, senderData->lengths, field, dst, value, len);
}

static inline void psk_set_int(uint32_t *fields, uint8_t *lengths, unsigned field, int64_t *dst,
                               int64_t value, size_t defLen, bool fixedLen) {
    *fields |= PSK_FIELD_MASK(field);
    *dst = value;

    size_t len = defLen;

    if (!fixedLen) {
        len = psk_int_min_bytes(value);

        if (len < defLen)
            len = defLen;
    }

    lengths[field] = (uint8_t)len;
}

static inline void psk_set_uint(uint32_t *fields, uint8_t *lengths, unsigned field, uint64_t *dst,
                                uint64_t value, size_t defLen, bool fixedLen) {
    *fields |= PSK_FIELD_MASK(field);
    *dst = value;

    size_t len = defLen;

    if (!fixedLen) {
        len = psk_uint_min_bytes(value);

        if (len < defLen)
            len = defLen;
    }

    lengths[field] = (uint8_t)len;
}

static inline void psk_sender_set_uint(pskSenderData_t *senderData, pskSenderField_t field,
                                       uint64_t *dst, uint64_t value, size_t defLen,
                                       bool fixedLen) {
    psk_set_uint(&senderData->fields, senderData->lengths, field, dst, value, defLen, fixedLen);
}

static inline void psk_sender_set_int(pskSenderData_t *senderData, pskSenderField_t field,
                                      int64_t *dst, int64_t value, size_t defLen, bool fixedLen) {
    psk_set_int(&senderData->fields, senderData->lengths, field, dst, value, defLen, fixedLen);
}

static inline void psk_set_bytes(uint32_t *fields, uint8_t *lengths, unsigned field,
                                 const uint8_t **dst, const uint8_t *value, size_t len) {
    *fields |= PSK_FIELD_MASK(field);
    *dst = value;
    lengths[field] = (uint8_t)len;
}

static inline void psk_sender_set_bytes(pskSenderData_t *senderData, pskSenderField_t field,
                                        const uint8_t **dst, const uint8_t *value, size_t len) {
    psk_set_bytes(&senderData->fields, senderData->lengths, field, dst, value, len);
}

pskError_t psk_receiver_data_set_callsign(pskReceiverData_t *receiverData, const char *callsign,
                                          size_t len) {
    if (!receiverData || !callsign)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_receiver_set_str(receiverData, PSK_RECEIVER_FIELD_CALLSIGN, &receiverData->callsign,
                         callsign, len);

    return PSK_ERR_OK;
}

pskError_t psk_receiver_data_set_locator(pskReceiverData_t *receiverData, const char *locator,
                                         size_t len) {
    if (!receiverData || !locator)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    if (!check_locator(locator, len))
        return PSK_ERR_INVALID_ARGS;

    psk_receiver_set_str(receiverData, PSK_RECEIVER_FIELD_LOCATOR, &receiverData->locator, locator,
                         len);

    return PSK_ERR_OK;
}

pskError_t psk_receiver_data_set_decoder_software(pskReceiverData_t *receiverData,
                                                  const char *decoderSoftware, size_t len) {
    if (!receiverData || !decoderSoftware)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_receiver_set_str(receiverData, PSK_RECEIVER_FIELD_DECODER_SOFTWARE,
                         &receiverData->decoderSoftware, decoderSoftware, len);

    return PSK_ERR_OK;
}

pskError_t psk_receiver_data_set_antenna_info(pskReceiverData_t *receiverData,
                                              const char *antennaInfo, size_t len) {
    if (!receiverData || !antennaInfo)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_receiver_set_str(receiverData, PSK_RECEIVER_FIELD_ANTENNA_INFO, &receiverData->antennaInfo,
                         antennaInfo, len);

    return PSK_ERR_OK;
}

pskError_t psk_receiver_data_set_persistent_id(pskReceiverData_t *receiverData,
                                               const char *persistentId, size_t len) {
    if (!receiverData || !persistentId)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_receiver_set_str(receiverData, PSK_RECEIVER_FIELD_PERSISTENT_ID,
                         &receiverData->persistentId, persistentId, len);

    return PSK_ERR_OK;
}

pskError_t psk_receiver_data_set_rig_info(pskReceiverData_t *receiverData, const char *rigInfo,
                                          size_t len) {
    if (!receiverData || !rigInfo)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_receiver_set_str(receiverData, PSK_RECEIVER_FIELD_RIG_INFO, &receiverData->rigInfo, rigInfo,
                         len);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_callsign(pskSenderData_t *senderData, const char *callsign,
                                        size_t len) {
    if (!senderData || !callsign)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_str(senderData, PSK_SENDER_FIELD_CALLSIGN, &senderData->callsign, callsign, len);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_locator(pskSenderData_t *senderData, const char *locator,
                                       size_t len) {
    if (!senderData || !locator)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    if (!check_locator(locator, len))
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_str(senderData, PSK_SENDER_FIELD_LOCATOR, &senderData->locator, locator, len);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_frequency(pskSenderData_t *senderData, uint64_t frequency) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_uint(senderData, PSK_SENDER_FIELD_FREQUENCY, &senderData->frequency, frequency,
                        4, false);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_snr(pskSenderData_t *senderData, int64_t snr) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_int(senderData, PSK_SENDER_FIELD_SNR, &senderData->snr, snr, 1, false);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_imd(pskSenderData_t *senderData, int64_t imd) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_int(senderData, PSK_SENDER_FIELD_IMD, &senderData->imd, imd, 1, false);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_mode(pskSenderData_t *senderData, const char *mode, size_t len) {
    if (!senderData || !mode)
        return PSK_ERR_INVALID_ARGS;

    if (len > PSK_MAX_STR_LEN)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_str(senderData, PSK_SENDER_FIELD_MODE, &senderData->mode, mode, len);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_info_src(pskSenderData_t *senderData, pskInfoSrc_t infoSrc,
                                        bool testTransmission) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    uint64_t infoSrcVal = infoSrc;
    if (testTransmission)
        infoSrcVal |= PSK_INFO_SRC_TEST_MASK;

    psk_sender_set_uint(senderData, PSK_SENDER_FIELD_INFO_SRC, &senderData->infoSrc, infoSrcVal, 1,
                        true);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_flow_start_secs(pskSenderData_t *senderData,
                                               uint64_t flowStartSecs) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_uint(senderData, PSK_SENDER_FIELD_FLOW_START_SECS, &senderData->flowStartSecs,
                        flowStartSecs, 4, false);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_message_bits(pskSenderData_t *senderData, const uint8_t *bytes,
                                            size_t len) {
    if (!senderData || !bytes)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_bytes(senderData, PSK_SENDER_FIELD_MESSAGE_BITS, &senderData->messageBits, bytes,
                         len);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_delta_time(pskSenderData_t *senderData, int16_t deltaTime) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    if (deltaTime == INT16_MIN)
        return PSK_ERR_INVALID_ARGS;

    psk_sender_set_int(senderData, PSK_SENDER_FIELD_DELTA_TIME, &senderData->deltaTime,
                       (int64_t)deltaTime, 2, true);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_fractional_frequency_8(pskSenderData_t *senderData,
                                                      double fractionalFrequency) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    if (fractionalFrequency < 0.0 || fractionalFrequency >= 1.0)
        return PSK_ERR_INVALID_ARGS;

    uint8_t fractionalFrequencyVal = (uint8_t)lround(fractionalFrequency * 256.0);

    psk_sender_set_uint(senderData, PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY,
                        &senderData->fractionalFrequency, (uint64_t)fractionalFrequencyVal, 1,
                        true);

    return PSK_ERR_OK;
}

pskError_t psk_sender_data_set_fractional_frequency_16(pskSenderData_t *senderData,
                                                       double fractionalFrequency) {
    if (!senderData)
        return PSK_ERR_INVALID_ARGS;

    if (fractionalFrequency < 0.0 || fractionalFrequency >= 1.0)
        return PSK_ERR_INVALID_ARGS;

    uint16_t fractionalFrequencyVal = (uint16_t)lround(fractionalFrequency * 65536.0);

    psk_sender_set_uint(senderData, PSK_SENDER_FIELD_FRACTIONAL_FREQUENCY,
                        &senderData->fractionalFrequency, (uint64_t)fractionalFrequencyVal, 2,
                        true);

    return PSK_ERR_OK;
}

pskError_t psk_templates_init(pskTemplates_t *templates) {
    templates->receiverTemplateCount = 0;
    templates->senderTemplateCount = 0;
    templates->receiverTemplateMax = PSK_MAX_RECEIVER_TEMPLATES;
    templates->senderTemplateMax = PSK_MAX_SENDER_TEMPLATES;

    templates->nextTemplateId = PSK_INITIAL_TEMPLATE_NUM;

    return PSK_ERR_OK;
}

pskError_t psk_templates_add(pskTemplates_t *templates, pskSetType_t setType, const uint8_t *rfdKey,
                             pskTemplate_t **outTemplate) {
    if (!templates || !rfdKey || !outTemplate)
        return PSK_ERR_INVALID_ARGS;

    pskTemplate_t *templatesSet = NULL;
    size_t *templateCount = NULL;
    size_t templateMax = 0;

    if (setType == PSK_SET_RECEIVER) {
        templatesSet = templates->receiverTemplate;
        templateCount = &templates->receiverTemplateCount;
        templateMax = templates->receiverTemplateMax;
    }
    else {
        templatesSet = templates->senderTemplate;
        templateCount = &templates->senderTemplateCount;
        templateMax = templates->senderTemplateMax;
    }

    if (templates->nextTemplateId > PSK_FINAL_TEMPLATE_NUM)
        return PSK_ERR_TEMPLATES_FULL;

    if (*templateCount >= templateMax)
        return PSK_ERR_TEMPLATES_FULL;

    pskTemplate_t *writeTemplate = &templatesSet[*templateCount];

    binio_write_bytes(writeTemplate->key, rfdKey, PSK_TEMPLATE_KEY_LEN);
    writeTemplate->templateId = templates->nextTemplateId++;
    writeTemplate->startupCount = 0;
    writeTemplate->lastSent = 0;

    (*templateCount)++;

    *outTemplate = writeTemplate;

    return PSK_ERR_OK;
}

pskError_t psk_templates_find(pskTemplates_t *templates, pskSetType_t setType,
                              const uint8_t *rfdKey, pskTemplate_t **outTemplate) {
    if (!templates || !rfdKey || !outTemplate)
        return PSK_ERR_INVALID_ARGS;

    pskTemplate_t *templatesSet = NULL;
    size_t templateCount = 0;

    if (setType == PSK_SET_RECEIVER) {
        templatesSet = templates->receiverTemplate;
        templateCount = templates->receiverTemplateCount;
    }
    else {
        templatesSet = templates->senderTemplate;
        templateCount = templates->senderTemplateCount;
    }

    for (size_t i = 0; i < templateCount; ++i) {
        if (memcmp(templatesSet[i].key, rfdKey, PSK_TEMPLATE_KEY_LEN) == 0) {
            *outTemplate = &templatesSet[i];
            return PSK_ERR_OK;
        }
    }

    *outTemplate = NULL;
    return PSK_ERR_TEMPLATE_NOT_FOUND;
}

void psk_templates_reset(pskTemplates_t *templates) {
    if (!templates)
        return;

    templates->receiverTemplateCount = 0;
    templates->senderTemplateCount = 0;
    templates->nextTemplateId = PSK_INITIAL_TEMPLATE_NUM;
}

pskError_t psk_ctx_init(pskCtx_t *ctx, const char *host, const char *port) {
    ctx->sequenceNum = 0;
    if (!hal_system_random_u32(&ctx->sessionId))
        return PSK_ERR_SYSTEM;
    ctx->activeSenderTemplate = NULL;
    ctx->activeReceiverTemplate = NULL;
    ctx->currentReceiverData = NULL;
    ctx->receiverRfdBuffered = false;
    ctx->senderRfdBuffered = false;

    halUdpErr_t udpErr = hal_udp_init(&ctx->udpCtx, host, port);
    if (udpErr != HAL_UDP_ERR_OK)
        return PSK_ERR_NETWORK;

    pskError_t rc;

    rc = psk_buf_init(&ctx->buf, PSK_PACKET_LEN);
    if (rc != PSK_ERR_OK)
        return rc;

    rc = psk_templates_init(&ctx->templates);
    if (rc != PSK_ERR_OK) {
        psk_buf_free(&ctx->buf);
        return rc;
    }

    return PSK_ERR_OK;
}

void psk_ctx_free(pskCtx_t *ctx) {
    ctx->sequenceNum = 0;
    ctx->sessionId = 0;
    ctx->activeSenderTemplate = NULL;
    ctx->activeReceiverTemplate = NULL;
    ctx->currentReceiverData = NULL;
    ctx->receiverRfdBuffered = false;
    ctx->senderRfdBuffered = false;

    psk_buf_free(&ctx->buf);

    hal_udp_cleanup(ctx->udpCtx);
}

pskError_t psk_ctx_reset(pskCtx_t *ctx) {
    if (!ctx)
        return PSK_ERR_INVALID_ARGS;

    if (!hal_system_random_u32(&ctx->sessionId))
        return PSK_ERR_SYSTEM;

    ctx->sequenceNum = 0;
    ctx->activeSenderTemplate = NULL;
    ctx->activeReceiverTemplate = NULL;
    ctx->receiverRfdBuffered = false;
    ctx->senderRfdBuffered = false;

    psk_buf_reset(&ctx->buf);
    psk_templates_reset(&ctx->templates);

    return PSK_ERR_OK;
}

pskError_t psk_ctx_send_manually(pskCtx_t *ctx) {
    if (!ctx)
        return PSK_ERR_INVALID_ARGS;

    if (ctx->buf.len == 0)
        return PSK_ERR_OK;

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PSK_ERR_SYSTEM;

    pskError_t rc;
    rc = psk_write_packet_header(&ctx->buf, ctx->buf.len, (uint32_t)currentTime, ctx->sequenceNum,
                                 ctx->sessionId);
    if (rc != PSK_ERR_OK)
        return rc;

    if (hal_udp_send(ctx->udpCtx, ctx->buf.data, ctx->buf.len) != HAL_UDP_ERR_OK)
        return PSK_ERR_NETWORK;

    ctx->sequenceNum++;

    if (ctx->receiverRfdBuffered && ctx->activeReceiverTemplate) {
        ctx->activeReceiverTemplate->startupCount++;
        ctx->activeReceiverTemplate->lastSent = currentTime;
    }
    ctx->receiverRfdBuffered = false;
    ctx->activeReceiverTemplate = NULL;

    if (ctx->senderRfdBuffered && ctx->activeSenderTemplate) {
        ctx->activeSenderTemplate->startupCount++;
        ctx->activeSenderTemplate->lastSent = currentTime;
    }
    ctx->senderRfdBuffered = false;
    ctx->activeSenderTemplate = NULL;

    psk_buf_reset(&ctx->buf);

    return PSK_ERR_OK;
}

pskError_t psk_ctx_set_receiver(pskCtx_t *ctx, const pskReceiverData_t *receiverData) {
    if (!ctx || !receiverData)
        return PSK_ERR_INVALID_ARGS;

    if ((receiverData->fields & PSK_RECEIVER_MANDATORY_FIELDS_MASK) !=
        PSK_RECEIVER_MANDATORY_FIELDS_MASK)
        return PSK_ERR_MISSING_FIELDS;

    pskError_t rc;

    if (ctx->activeSenderTemplate) {
        rc = psk_ctx_send_manually(ctx);
        if (rc != PSK_ERR_OK)
            return rc;
    }
    else {
        psk_buf_reset(&ctx->buf);
    }

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PSK_ERR_SYSTEM;

    rc = psk_write_packet_header(&ctx->buf, 0, 0, 0, 0);
    if (rc != PSK_ERR_OK) {
        psk_buf_reset(&ctx->buf);
        return rc;
    }

    uint8_t rfdKey[PSK_TEMPLATE_KEY_LEN];
    rc = psk_generate_receiver_rfd_key(receiverData, rfdKey);
    if (rc != PSK_ERR_OK)
        return rc;

    pskTemplate_t *foundTemplate;

    rc = psk_templates_find(&ctx->templates, PSK_SET_RECEIVER, rfdKey, &foundTemplate);
    if (rc != PSK_ERR_OK && rc != PSK_ERR_TEMPLATE_NOT_FOUND)
        return rc;

    if (rc == PSK_ERR_TEMPLATE_NOT_FOUND) {
        rc = psk_templates_add(&ctx->templates, PSK_SET_RECEIVER, rfdKey, &foundTemplate);
        if (rc != PSK_ERR_OK && rc != PSK_ERR_TEMPLATES_FULL)
            return rc;

        if (rc == PSK_ERR_TEMPLATES_FULL) {
            rc = psk_ctx_reset(ctx);
            if (rc != PSK_ERR_OK)
                return rc;

            return psk_ctx_set_receiver(ctx, receiverData);
        }
    }

    if (foundTemplate->startupCount <= PSK_RFD_NUM_STARTUP ||
        (currentTime - foundTemplate->lastSent) >= PSK_RFD_SEC_TIMEOUT) {

        rc = psk_append_receiver_rfd(&ctx->buf, receiverData, foundTemplate->templateId);

        if (rc != PSK_ERR_OK) {
            psk_buf_reset(&ctx->buf);
            return rc;
        }
    }

    rc = psk_append_receiver_data(&ctx->buf, receiverData, foundTemplate->templateId);
    if (rc != PSK_ERR_OK) {
        psk_buf_reset(&ctx->buf);
        return rc;
    }

    ctx->activeReceiverTemplate = foundTemplate;
    ctx->currentReceiverData = receiverData;

    return PSK_ERR_OK;
}

pskError_t psk_ctx_add_sender(pskCtx_t *ctx, const pskSenderData_t *senderData) {
    if (!ctx || !senderData)
        return PSK_ERR_INVALID_ARGS;

    if ((senderData->fields & PSK_SENDER_MANDATORY_FIELDS_MASK) != PSK_SENDER_MANDATORY_FIELDS_MASK)
        return PSK_ERR_MISSING_FIELDS;

    if (!ctx->currentReceiverData)
        return PSK_ERR_INVALID_STATE;

    uint64_t currentTime;
    if (!hal_system_time_unix_u64(&currentTime))
        return PSK_ERR_SYSTEM;

    pskError_t rc;

    uint8_t rfdKey[PSK_TEMPLATE_KEY_LEN];
    rc = psk_generate_sender_rfd_key(senderData, rfdKey);
    if (rc != PSK_ERR_OK)
        return rc;

    pskTemplate_t *foundTemplate;

    rc = psk_templates_find(&ctx->templates, PSK_SET_SENDER, rfdKey, &foundTemplate);
    if (rc != PSK_ERR_OK && rc != PSK_ERR_TEMPLATE_NOT_FOUND)
        return rc;

    if (rc == PSK_ERR_TEMPLATE_NOT_FOUND) {
        rc = psk_templates_add(&ctx->templates, PSK_SET_SENDER, rfdKey, &foundTemplate);
        if (rc != PSK_ERR_OK && rc != PSK_ERR_TEMPLATES_FULL)
            return rc;

        if (rc == PSK_ERR_TEMPLATES_FULL) {
            rc = psk_ctx_send_manually(ctx);
            if (rc != PSK_ERR_OK)
                return rc;

            rc = psk_ctx_reset(ctx);
            if (rc != PSK_ERR_OK)
                return rc;

            rc = psk_ctx_set_receiver(ctx, ctx->currentReceiverData);
            if (rc != PSK_ERR_OK)
                return rc;

            return psk_ctx_add_sender(ctx, senderData);
        }
    }

    if (ctx->activeSenderTemplate && ctx->activeSenderTemplate == foundTemplate) {
        rc = psk_append_sender_data(&ctx->buf, senderData, foundTemplate->templateId);
        if (rc != PSK_ERR_BUF_TOO_SMALL)
            return rc;
    }

    rc = psk_ctx_set_receiver(ctx, ctx->currentReceiverData);
    if (rc != PSK_ERR_OK)
        return rc;

    size_t rollbackLen = ctx->buf.len;
    if (foundTemplate->startupCount <= PSK_RFD_NUM_STARTUP ||
        (currentTime - foundTemplate->lastSent) >= PSK_RFD_SEC_TIMEOUT) {

        rc = psk_append_sender_rfd(&ctx->buf, senderData, foundTemplate->templateId);
        if (rc != PSK_ERR_OK)
            return rc;
    }

    rc = psk_append_sender_data(&ctx->buf, senderData, foundTemplate->templateId);
    if (rc != PSK_ERR_OK) {
        ctx->buf.len = rollbackLen;
        return rc;
    }

    ctx->activeSenderTemplate = foundTemplate;

    return PSK_ERR_OK;
}

int main() {
    pskReceiverData_t receiverData = {0};
    const char *receiverCallsign = "EA4IGV";
    const char *receiverLocator = "IN80";
    const char *decoderSoftware = "pskLibC";

    psk_receiver_data_set_callsign(&receiverData, receiverCallsign, strlen(receiverCallsign));
    psk_receiver_data_set_locator(&receiverData, receiverLocator, strlen(receiverLocator));
    psk_receiver_data_set_decoder_software(&receiverData, decoderSoftware, strlen(decoderSoftware));

    pskSenderData_t senderData = {0};
    const char *senderCallsign = "EA1ZXZ";
    const char *senderMode = PSK_MODE_FT4;

    psk_sender_data_set_callsign(&senderData, senderCallsign, strlen(senderCallsign));
    psk_sender_data_set_frequency(&senderData, 7145000);
    psk_sender_data_set_mode(&senderData, senderMode, strlen(senderMode));
    psk_sender_data_set_info_src(&senderData, PSK_INFO_SRC_MANUAL, true);
    psk_sender_data_set_flow_start_secs(&senderData, time(NULL));
    psk_sender_data_set_snr(&senderData, -5);

    pskCtx_t pskCtx;
    pskError_t rc;

    rc = psk_ctx_init(&pskCtx, PSK_DEFAULT_HOST, PSK_TEST_PORT);
    if (rc != PSK_ERR_OK) {
        printf("Error in psk_ctx_init, %d\n", rc);
        return -1;
    }

    rc = psk_ctx_set_receiver(&pskCtx, &receiverData);
    if (rc != PSK_ERR_OK) {
        printf("Error in psk_ctx_set_receiver, %d\n", rc);
        return -1;
    }

    rc = psk_ctx_add_sender(&pskCtx, &senderData);
    if (rc != PSK_ERR_OK) {
        printf("Error in psk_ctx_add_sender, %d\n", rc);
        return -1;
    }

    rc = psk_ctx_send_manually(&pskCtx);
    if (rc != PSK_ERR_OK) {
        printf("Error in psk_ctx_send_manually, %d\n", rc);
        return -1;
    }

    psk_ctx_free(&pskCtx);
}
