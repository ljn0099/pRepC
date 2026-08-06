#ifndef PREPC_LIB_C_H
#define PREPC_LIB_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PREPC_DEFAULT_HOST "report.prepcreporter.info"
#define PREPC_DEFAULT_PORT "4739"
#define PREPC_TEST_PORT "14739"

typedef enum {
    PREPC_RECEIVER_FIELD_CALLSIGN = 0,
    PREPC_RECEIVER_FIELD_LOCATOR,
    PREPC_RECEIVER_FIELD_DECODER_SOFTWARE,
    PREPC_RECEIVER_FIELD_ANTENNA_INFO,
    PREPC_RECEIVER_FIELD_PERSISTENT_ID,
    PREPC_RECEIVER_FIELD_RIG_INFO,

    PREPC_RECEIVER_FIELD_COUNT
} prepcReceiverField_t;

typedef enum {
    PREPC_SENDER_FIELD_CALLSIGN = 0,
    PREPC_SENDER_FIELD_LOCATOR,
    PREPC_SENDER_FIELD_FREQUENCY,
    PREPC_SENDER_FIELD_SNR,
    PREPC_SENDER_FIELD_IMD,
    PREPC_SENDER_FIELD_MODE,
    PREPC_SENDER_FIELD_INFO_SRC,
    PREPC_SENDER_FIELD_FLOW_START_SECS,
    PREPC_SENDER_FIELD_MESSAGE_BITS,
    PREPC_SENDER_FIELD_DELTA_TIME,
    PREPC_SENDER_FIELD_FRACTIONAL_FREQUENCY,

    PREPC_SENDER_FIELD_COUNT
} prepcSenderField_t;

#define PREPC_TEMPLATE_KEY_LEN                                                                       \
    (sizeof(prepcFields_t) + (((size_t)PREPC_SENDER_FIELD_COUNT > (size_t)PREPC_RECEIVER_FIELD_COUNT)    \
                                ? (size_t)PREPC_SENDER_FIELD_COUNT                                   \
                                : (size_t)PREPC_RECEIVER_FIELD_COUNT))

typedef enum {
    PREPC_INFO_SRC_AUTO = 1,
    PREPC_INFO_SRC_CALL_LOG = 2,
    PREPC_INFO_SRC_MANUAL = 3,
} prepcInfoSrc_t;

typedef uint32_t prepcFields_t;

typedef struct {
    prepcFields_t fields;
    uint8_t lengths[PREPC_RECEIVER_FIELD_COUNT];

    const char *callsign;
    const char *locator;
    const char *decoderSoftware;
    const char *antennaInfo;
    const char *persistentId;
    const char *rigInfo;
} prepcReceiverData_t;

typedef struct {
    prepcFields_t fields;
    uint8_t lengths[PREPC_SENDER_FIELD_COUNT];

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
} prepcSenderData_t;

typedef struct {
    uint8_t key[PREPC_TEMPLATE_KEY_LEN];
    uint16_t templateId;
    uint8_t startupCount;
    uint64_t lastSent;
} prepcTemplate_t;

#define PREPC_MAX_SENDER_TEMPLATES 15
#define PREPC_MAX_RECEIVER_TEMPLATES 15

typedef struct {
    prepcTemplate_t receiverTemplate[PREPC_MAX_RECEIVER_TEMPLATES];
    size_t receiverTemplateCount;
    size_t receiverTemplateMax;

    prepcTemplate_t senderTemplate[PREPC_MAX_SENDER_TEMPLATES];
    size_t senderTemplateCount;
    size_t senderTemplateMax;

    uint16_t nextTemplateId;
} prepcTemplates_t;

typedef struct {
    uint8_t *data;
    size_t len;
    size_t maxLen;
} prepcBuf_t;

typedef struct {
    uint32_t sequenceNum;
    uint32_t sessionId;

    prepcTemplate_t *activeReceiverTemplate;
    prepcTemplate_t *activeSenderTemplate;
    bool receiverRfdBuffered;
    bool senderRfdBuffered;

    prepcBuf_t buf;
    prepcTemplates_t templates;

    const prepcReceiverData_t *currentReceiverData;

    uint64_t lastDNSSync;

    void *udpCtx;
} prepcCtx_t;

typedef enum {
    PREPC_ERR_OK = 0,
    PREPC_ERR_UNKNOWN,
    PREPC_ERR_BUF_TOO_SMALL,
    PREPC_ERR_INVALID_ARGS,
    PREPC_ERR_MEMORY,
    PREPC_ERR_SYSTEM,
    PREPC_ERR_TEMPLATES_FULL,
    PREPC_ERR_TEMPLATE_NOT_FOUND,
    PREPC_ERR_NETWORK,
    PREPC_ERR_INVALID_STATE,
    PREPC_ERR_MISSING_FIELDS
} prepcError_t;

prepcError_t prepc_ctx_init(prepcCtx_t *ctx, const char *host, const char *port);

prepcError_t prepc_ctx_set_receiver(prepcCtx_t *ctx, const prepcReceiverData_t *receiverData);

prepcError_t prepc_ctx_add_sender(prepcCtx_t *ctx, const prepcSenderData_t *senderData);

prepcError_t prepc_ctx_send_manually(prepcCtx_t *ctx);

void prepc_ctx_free(prepcCtx_t *ctx);

prepcError_t prepc_receiver_data_set_callsign(prepcReceiverData_t *receiverData,
                                                        const char *callsign, size_t len);

prepcError_t prepc_receiver_data_set_locator(prepcReceiverData_t *receiverData, const char *locator,
                                         size_t len);

prepcError_t prepc_receiver_data_set_decoder_software(prepcReceiverData_t *receiverData,
                                                  const char *decoderSoftware, size_t len);

prepcError_t prepc_receiver_data_set_antenna_info(prepcReceiverData_t *receiverData,
                                              const char *antennaInfo, size_t len);

prepcError_t prepc_receiver_data_set_persistent_id(prepcReceiverData_t *receiverData,
                                               const char *persistentId, size_t len);

prepcError_t prepc_receiver_data_set_rig_info(prepcReceiverData_t *receiverData, const char *rigInfo,
                                          size_t len);

// Sender Data
prepcError_t prepc_sender_data_set_callsign(prepcSenderData_t *senderData, const char *callsign,
                                        size_t len);

prepcError_t prepc_sender_data_set_locator(prepcSenderData_t *senderData, const char *locator,
                                       size_t len);

prepcError_t prepc_sender_data_set_frequency(prepcSenderData_t *senderData, uint64_t frequency);

prepcError_t prepc_sender_data_set_snr(prepcSenderData_t *senderData, int64_t snr);

prepcError_t prepc_sender_data_set_imd(prepcSenderData_t *senderData, int64_t imd);

prepcError_t prepc_sender_data_set_mode(prepcSenderData_t *senderData, const char *mode, size_t len);

prepcError_t prepc_sender_data_set_info_src(prepcSenderData_t *senderData, prepcInfoSrc_t infoSrc,
                                        bool testTransmission);

prepcError_t prepc_sender_data_set_flow_start_secs(prepcSenderData_t *senderData, uint64_t flowStartSecs);

prepcError_t prepc_sender_data_set_message_bits(prepcSenderData_t *senderData, const uint8_t *bytes,
                                            size_t len);

prepcError_t prepc_sender_data_set_delta_time(prepcSenderData_t *senderData, int16_t deltaTime);

prepcError_t prepc_sender_data_set_fractional_frequency_8(prepcSenderData_t *senderData,
                                                      double fractionalFrequency);

prepcError_t prepc_sender_data_set_fractional_frequency_16(prepcSenderData_t *senderData,
                                                       double fractionalFrequency);

#endif
