#ifndef PSK_UDP_HAL_H
#define PSK_UDP_HAL_H

#include <stddef.h>
#include <stdint.h>

#define HOST_MAX_LEN 256
#define PORT_MAX_LEN 6
#define HAL_UDP_MAX_BUFFERS 5

typedef enum {
    HAL_UDP_ERR_OK = 0,
    HAL_UDP_ERR_OK_HOST_CHANGED,
    HAL_UDP_ERR_INVALID_ARGUMENT,
    HAL_UDP_ERR_NETWORK,
    HAL_UDP_ERR_RESOLVE,
    HAL_UDP_ERR_INTERNAL
} halUdpErr_t;

typedef struct {
    const uint8_t *data;
    size_t len;
} halUdpBuf_t;

halUdpErr_t hal_udp_init(void **context, const char *host, const char *port);

halUdpErr_t hal_udp_reresolve(void *context);

halUdpErr_t hal_udp_send(void *context, const uint8_t *data, size_t len);

halUdpErr_t hal_udp_send_vector(void *context, const halUdpBuf_t *buffers, size_t count);

void hal_udp_cleanup(void *context);
#endif
