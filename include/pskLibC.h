#ifndef PSK_LIB_C_H
#define PSK_LIB_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PSK_DEFAULT_HOST "report.pskreporter.info"
#define PSK_DEFAULT_PORT "4739"
#define PSK_TEST_PORT "14739"

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

#define PSK_TEMPLATE_KEY_LEN                                                                       \
    (sizeof(pskFields_t) + (((size_t)PSK_SENDER_FIELD_COUNT > (size_t)PSK_RECEIVER_FIELD_COUNT)    \
                                ? (size_t)PSK_SENDER_FIELD_COUNT                                   \
                                : (size_t)PSK_RECEIVER_FIELD_COUNT))

typedef enum {
    PSK_INFO_SRC_AUTO = 1,
    PSK_INFO_SRC_CALL_LOG = 2,
    PSK_INFO_SRC_MANUAL = 3,
} pskInfoSrc_t;

typedef uint32_t pskFields_t;

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

#define PSK_MAX_SENDER_TEMPLATES 15
#define PSK_MAX_RECEIVER_TEMPLATES 15

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

    uint64_t lastDNSSync;

    void *udpCtx;
} pskCtx_t;

typedef enum {
    PSK_ERR_OK = 0,
    PSK_ERR_UNKNOWN,
    PSK_ERR_BUF_TOO_SMALL,
    PSK_ERR_INVALID_ARGS,
    PSK_ERR_MEMORY,
    PSK_ERR_SYSTEM,
    PSK_ERR_TEMPLATES_FULL,
    PSK_ERR_TEMPLATE_NOT_FOUND,
    PSK_ERR_NETWORK,
    PSK_ERR_INVALID_STATE,
    PSK_ERR_MISSING_FIELDS
} pskError_t;

pskError_t psk_ctx_init(pskCtx_t *ctx, const char *host, const char *port);

pskError_t psk_ctx_set_receiver(pskCtx_t *ctx, const pskReceiverData_t *receiverData);

pskError_t psk_ctx_add_sender(pskCtx_t *ctx, const pskSenderData_t *senderData);

pskError_t psk_ctx_send_manually(pskCtx_t *ctx);

void psk_ctx_free(pskCtx_t *ctx);

pskError_t psk_receiver_data_set_callsign(pskReceiverData_t *receiverData,
                                                        const char *callsign, size_t len);

pskError_t psk_receiver_data_set_locator(pskReceiverData_t *receiverData, const char *locator,
                                         size_t len);

pskError_t psk_receiver_data_set_decoder_software(pskReceiverData_t *receiverData,
                                                  const char *decoderSoftware, size_t len);

pskError_t psk_receiver_data_set_antenna_info(pskReceiverData_t *receiverData,
                                              const char *antennaInfo, size_t len);

pskError_t psk_receiver_data_set_persistent_id(pskReceiverData_t *receiverData,
                                               const char *persistentId, size_t len);

pskError_t psk_receiver_data_set_rig_info(pskReceiverData_t *receiverData, const char *rigInfo,
                                          size_t len);

// Sender Data
pskError_t psk_sender_data_set_callsign(pskSenderData_t *senderData, const char *callsign,
                                        size_t len);

pskError_t psk_sender_data_set_locator(pskSenderData_t *senderData, const char *locator,
                                       size_t len);

pskError_t psk_sender_data_set_frequency(pskSenderData_t *senderData, uint64_t frequency);

pskError_t psk_sender_data_set_snr(pskSenderData_t *senderData, int64_t snr);

pskError_t psk_sender_data_set_imd(pskSenderData_t *senderData, int64_t imd);

pskError_t psk_sender_data_set_mode(pskSenderData_t *senderData, const char *mode, size_t len);

pskError_t psk_sender_data_set_info_src(pskSenderData_t *senderData, pskInfoSrc_t infoSrc,
                                        bool testTransmission);

pskError_t psk_sender_data_set_flow_start_secs(pskSenderData_t *senderData, uint64_t flowStartSecs);

pskError_t psk_sender_data_set_message_bits(pskSenderData_t *senderData, const uint8_t *bytes,
                                            size_t len);

pskError_t psk_sender_data_set_delta_time(pskSenderData_t *senderData, int16_t deltaTime);

pskError_t psk_sender_data_set_fractional_frequency_8(pskSenderData_t *senderData,
                                                      double fractionalFrequency);

pskError_t psk_sender_data_set_fractional_frequency_16(pskSenderData_t *senderData,
                                                       double fractionalFrequency);

#endif
