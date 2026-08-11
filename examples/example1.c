#include "pRepC.h"
#include "pRepCLimiter.h"
#include "pRepCModes.h"
#include "pRepCSystem.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    prepcReceiverData_t receiverData = {0};
    const char *receiverCallsign = "EA4ZZZ";
    const char *receiverLocator = "IN80";
    const char *decoderSoftware = "prepcLibC";

    prepc_receiver_data_set_callsign(&receiverData, receiverCallsign, strlen(receiverCallsign));
    prepc_receiver_data_set_locator(&receiverData, receiverLocator, strlen(receiverLocator));
    prepc_receiver_data_set_decoder_software(&receiverData, decoderSoftware,
                                             strlen(decoderSoftware));

    prepcCtx_t *prepcCtx;
    prepcRateCtx_t *prepcRateCtx;
    prepcError_t rc;

    rc = prepc_ctx_init(&prepcCtx, PREPC_DEFAULT_HOST, PREPC_TEST_PORT);
    if (rc != PREPC_ERR_OK) {
        printf("Error in prepc_ctx_init, %d\n", rc);
        return -1;
    }

    rc = prepc_rate_ctx_init(&prepcRateCtx, 512);
    if (rc != PREPC_ERR_OK) {
        printf("Error in prepc_rate_ctx_init, %d\n", rc);
        return -1;
    }

    rc = prepc_ctx_set_receiver(prepcCtx, &receiverData);
    if (rc != PREPC_ERR_OK) {
        printf("Error in prepc_ctx_set_receiver, %d\n", rc);
        prepc_ctx_free(prepcCtx);
        prepc_rate_free(prepcRateCtx);
        return -1;
    }

    prepcSenderData_t senderData = {0};
    char callsign[16];
    char mode[16];
    uint64_t freqHz;
    int64_t snr;
    uint64_t flowStartSecs;

    char line[128];

    printf("Introduce the spots with the fields separated by spaces in this order:\n");
    printf("CALLSIGN MODE FREQHz SNR\n");
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (sscanf(line, "%15s %15s %" SCNu64 " %" SCNd64, callsign, mode, &freqHz, &snr) != 4) {
            printf("Invalid line\n");
            continue;
        }

        // Get system time
        if (!prepc_system_time_unix_u64(&flowStartSecs)) {
            printf("Error in prepc_system_time_unix_u64\n");
            prepc_ctx_free(prepcCtx);
            prepc_rate_free(prepcRateCtx);
            return -1;
        }

        rc = prepc_rate_should_report(prepcRateCtx, callsign, strlen(callsign), mode, strlen(mode),
                                      freqHz, flowStartSecs);
        if (rc == PREPC_ERR_NOT_REPORT) {
            printf("Spot not reported\n");
            continue;
        }
        if (rc != PREPC_ERR_OK) {
            printf("Error in prepc_rate_should_report, %d\n", rc);
            prepc_ctx_free(prepcCtx);
            prepc_rate_free(prepcRateCtx);
            return -1;
        }

        prepc_sender_data_reset(&senderData);
        prepc_sender_data_set_callsign(&senderData, callsign, strlen(callsign));
        prepc_sender_data_set_mode(&senderData, mode, strlen(mode));
        prepc_sender_data_set_frequency(&senderData, freqHz);
        prepc_sender_data_set_snr(&senderData, snr);
        prepc_sender_data_set_info_src(&senderData, PREPC_INFO_SRC_AUTO, false);
        prepc_sender_data_set_flow_start_secs(&senderData, flowStartSecs);

        rc = prepc_ctx_add_sender(prepcCtx, &senderData);
        // You can try to call again the function and recover if it returns PREPC_ERR_NETWORK
        if (rc != PREPC_ERR_OK) {
            printf("Error in prepc_ctx_add_sender, %d\n", rc);
            prepc_ctx_free(prepcCtx);
            prepc_rate_free(prepcRateCtx);
            return -1;
        }

        // Try to send if last was sent more than 5 minutes ago
        rc = prepc_ctx_flush(prepcCtx, 300, false);
        // You can try to call again the function and recover if it returns PREPC_ERR_NETWORK
        if (rc != PREPC_ERR_OK) {
            printf("Error in prepc_ctx_send_manually, %d\n", rc);
            prepc_ctx_free(prepcCtx);
            prepc_rate_free(prepcRateCtx);
            return -1;
        }
    }

    rc = prepc_ctx_flush(prepcCtx, 0, false);
    // You can try to call again the function and recover if it returns PREPC_ERR_NETWORK
    if (rc != PREPC_ERR_OK) {
        printf("Error in prepc_ctx_send_manually, %d\n", rc);
        prepc_ctx_free(prepcCtx);
        prepc_rate_free(prepcRateCtx);
        return -1;
    }

    prepc_ctx_free(prepcCtx);
    prepc_rate_free(prepcRateCtx);
}
