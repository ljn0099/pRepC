#ifndef PREPC_UDP_HAL_H
#define PREPC_UDP_HAL_H

#include <stddef.h>
#include <stdint.h>

#define PREPC_UDP_HOST_MAX_LEN 256
#define PREPC_UDP_PORT_MAX_LEN 6

typedef enum {
    PREPC_UDP_ERR_OK = 0,
    PREPC_UDP_ERR_OK_HOST_CHANGED,
    PREPC_UDP_ERR_INVALID_ARGUMENT,
    PREPC_UDP_ERR_NETWORK,
    PREPC_UDP_ERR_RESOLVE,
    PREPC_UDP_ERR_INTERNAL
} prepcUdpErr_t;

prepcUdpErr_t prepc_udp_init(void **context, const char *host, const char *port);

prepcUdpErr_t prepc_udp_reresolve(void *context);

prepcUdpErr_t prepc_udp_send(void *context, const uint8_t *data, size_t len);

void prepc_udp_cleanup(void *context);
#endif
