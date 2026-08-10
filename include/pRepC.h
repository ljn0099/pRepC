#ifndef PREPC_LIB_C_H
#define PREPC_LIB_C_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PREPC_DEFAULT_HOST "report.pskreporter.info"
#define PREPC_DEFAULT_PORT "4739"
#define PREPC_TEST_PORT "14739"

#define PREPC_PACKET_LEN 1400

#define PREPC_MAX_SENDER_TEMPLATES 15
#define PREPC_MAX_RECEIVER_TEMPLATES 15

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

typedef enum {
    PREPC_INFO_SRC_AUTO = 1,
    PREPC_INFO_SRC_CALL_LOG = 2,
    PREPC_INFO_SRC_MANUAL = 3,
} prepcInfoSrc_t;

typedef struct prepcCtx_t prepcCtx_t;

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
    PREPC_ERR_FULL,
    PREPC_ERR_NOT_REPORT,
    PREPC_ERR_MISSING_FIELDS
} prepcError_t;

prepcError_t prepc_ctx_init(prepcCtx_t **ctx, const char *host, const char *port);

prepcError_t prepc_ctx_set_receiver(prepcCtx_t *ctx, const prepcReceiverData_t *receiverData);

prepcError_t prepc_ctx_add_sender(prepcCtx_t *ctx, const prepcSenderData_t *senderData);

prepcError_t prepc_ctx_flush(prepcCtx_t *ctx, uint64_t minIntervalSecs, bool allowReceiverDataOnly);

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

prepcError_t prepc_receiver_data_set_rig_info(prepcReceiverData_t *receiverData,
                                              const char *rigInfo, size_t len);

void prepc_receiver_data_reset(prepcReceiverData_t *receiverData);

// Sender Data
prepcError_t prepc_sender_data_set_callsign(prepcSenderData_t *senderData, const char *callsign,
                                            size_t len);

prepcError_t prepc_sender_data_set_locator(prepcSenderData_t *senderData, const char *locator,
                                           size_t len);

prepcError_t prepc_sender_data_set_frequency(prepcSenderData_t *senderData, uint64_t frequency);

prepcError_t prepc_sender_data_set_snr(prepcSenderData_t *senderData, int64_t snr);

prepcError_t prepc_sender_data_set_imd(prepcSenderData_t *senderData, int64_t imd);

prepcError_t prepc_sender_data_set_mode(prepcSenderData_t *senderData, const char *mode,
                                        size_t len);

prepcError_t prepc_sender_data_set_info_src(prepcSenderData_t *senderData, prepcInfoSrc_t infoSrc,
                                            bool testTransmission);

prepcError_t prepc_sender_data_set_flow_start_secs(prepcSenderData_t *senderData,
                                                   uint64_t flowStartSecs);

prepcError_t prepc_sender_data_set_message_bits(prepcSenderData_t *senderData, const uint8_t *bytes,
                                                size_t len);

prepcError_t prepc_sender_data_set_delta_time(prepcSenderData_t *senderData, int64_t deltaTimeUs);

prepcError_t prepc_sender_data_set_fractional_frequency_8(prepcSenderData_t *senderData,
                                                          double fractionalFrequency);

prepcError_t prepc_sender_data_set_fractional_frequency_16(prepcSenderData_t *senderData,
                                                           double fractionalFrequency);

void prepc_sender_data_reset(prepcSenderData_t *senderData);

#endif
