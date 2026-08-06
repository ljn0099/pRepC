#include "pskLibC.h"
#include "pskLibCModes.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
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
