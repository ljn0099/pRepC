#include "pRepC.h"
#include "pRepCLimiter.h"
#include "pRepCModes.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

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
        return -1;
    }

    prepcSenderData_t senderData = {0};
    const char *callsign = "EA1ZXZ";
    const char *mode = PREPC_MODE_FT4;
    uint64_t freqHz = 7145000;
    uint64_t flowStartSecs = (uint64_t)time(NULL);

    int c;
    while ((c = getchar()) != EOF) { // Press a key to continue
        rc = prepc_rate_should_report(prepcRateCtx, callsign, strlen(callsign), mode,
                                      strlen(mode), freqHz, flowStartSecs);

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
        prepc_sender_data_set_frequency(&senderData, freqHz);
        prepc_sender_data_set_mode(&senderData, mode, strlen(mode));
        prepc_sender_data_set_info_src(&senderData, PREPC_INFO_SRC_AUTO, true);
        prepc_sender_data_set_flow_start_secs(&senderData, flowStartSecs);

        rc = prepc_ctx_add_sender(prepcCtx, &senderData);
        if (rc != PREPC_ERR_OK) { // You can try to call again the function and recover if it
                                  // returns PREPC_ERR_NETWORK
            printf("Error in prepc_ctx_add_sender, %d\n", rc);
            prepc_ctx_free(prepcCtx);
            prepc_rate_free(prepcRateCtx);
            return -1;
        }
    }

    rc = prepc_ctx_flush(prepcCtx, 0, false);
    if (rc != PREPC_ERR_OK) {
        printf("Error in prepc_ctx_send_manually, %d\n", rc);
        prepc_ctx_free(prepcCtx);
        prepc_rate_free(prepcRateCtx);
        return -1;
    }

    prepc_ctx_free(prepcCtx);
    prepc_rate_free(prepcRateCtx);
}
