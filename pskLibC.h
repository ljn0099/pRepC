#ifndef PSK_LIB_C_H
#define PSK_LIB_C_H

#define PSK_DEFAULT_HOST "report.pskreporter.info"
#define PSK_DEFAULT_PORT "4739"
#define PSK_TEST_PORT "14739"

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

// pskError_t psk_ctx_init(pskCtx_t *ctx, const char *host, const char *port);

// pskError_t psk_ctx_set_receiver(pskCtx_t *ctx, const pskReceiverData_t *receiverData);

// pskError_t psk_ctx_add_sender(pskCtx_t *ctx, const pskSenderData_t *senderData);

// pskError_t psk_ctx_send_manually(pskCtx_t *ctx);

// void psk_ctx_free(pskCtx_t *ctx);

// Receiver data
// pskError_t psk_receiver_data_set_callsign(pskReceiverData_t *receiverData, const char *callsign,
//                                           size_t len);

// pskError_t psk_receiver_data_set_locator(pskReceiverData_t *receiverData, const char *locator,
//                                          size_t len);

// pskError_t psk_receiver_data_set_decoder_software(pskReceiverData_t *receiverData,
//                                                   const char *decoderSoftware, size_t len);

// pskError_t psk_receiver_data_set_antenna_info(pskReceiverData_t *receiverData,
//                                               const char *antennaInfo, size_t len);

// pskError_t psk_receiver_data_set_persistent_id(pskReceiverData_t *receiverData,
//                                                const char *persistentId, size_t len);

// pskError_t psk_receiver_data_set_rig_info(pskReceiverData_t *receiverData, const char *rigInfo,
//                                           size_t len);

// // Sender Data
// pskError_t psk_sender_data_set_callsign(pskSenderData_t *senderData, const char *callsign,
//                                         size_t len);

// pskError_t psk_sender_data_set_locator(pskSenderData_t *senderData, const char *locator,
//                                        size_t len);

// pskError_t psk_sender_data_set_frequency(pskSenderData_t *senderData, uint64_t frequency);

// pskError_t psk_sender_data_set_snr(pskSenderData_t *senderData, int64_t snr);

// pskError_t psk_sender_data_set_imd(pskSenderData_t *senderData, int64_t imd);

// pskError_t psk_sender_data_set_mode(pskSenderData_t *senderData, const char *mode, size_t len);

// pskError_t psk_sender_data_set_info_src(pskSenderData_t *senderData, pskInfoSrc_t infoSrc,
//                                         bool testTransmission);

// pskError_t psk_sender_data_set_flow_start_secs(pskSenderData_t *senderData, uint64_t flowStartSecs);

// pskError_t psk_sender_data_set_message_bits(pskSenderData_t *senderData, const uint8_t *bytes,
//                                             size_t len);

// pskError_t psk_sender_data_set_delta_time(pskSenderData_t *senderData, int16_t deltaTime);

// pskError_t psk_sender_data_set_fractional_frequency_8(pskSenderData_t *senderData,
//                                                       double fractionalFrequency);

// pskError_t psk_sender_data_set_fractional_frequency_16(pskSenderData_t *senderData,
//                                                        double fractionalFrequency);

#endif
