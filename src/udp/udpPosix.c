#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#include "udpHal.h"

typedef struct {
    int sock;

    char host[PREPC_UDP_HOST_MAX_LEN];
    char port[PREPC_UDP_PORT_MAX_LEN];

    struct sockaddr_storage addr;
    socklen_t addrlen;
} connContext_t;

static bool sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b) {
    if (!a || !b)
        return false;

    if (a->sa_family != b->sa_family)
        return false;

    if (a->sa_family == AF_INET) {
        const struct sockaddr_in *sa = (const struct sockaddr_in *)a;
        const struct sockaddr_in *sb = (const struct sockaddr_in *)b;

        return sa->sin_port == sb->sin_port &&
               memcmp(&sa->sin_addr, &sb->sin_addr, sizeof(sa->sin_addr)) == 0;
    }
    else if (a->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sa = (const struct sockaddr_in6 *)a;
        const struct sockaddr_in6 *sb = (const struct sockaddr_in6 *)b;

        return sa->sin6_port == sb->sin6_port && sa->sin6_scope_id == sb->sin6_scope_id &&
               memcmp(&sa->sin6_addr, &sb->sin6_addr, sizeof(sa->sin6_addr)) == 0;
    }

    return false;
}

static prepcUdpErr_t udp_reopen(connContext_t *ctx) {
    if (!ctx)
        return PREPC_UDP_ERR_INVALID_ARGUMENT;

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;

    int err = getaddrinfo(ctx->host, ctx->port, &hints, &res);
    if (err != 0) {
        if (err == EAI_AGAIN)
            return PREPC_UDP_ERR_NETWORK;

        if (err == EAI_NONAME)
            return PREPC_UDP_ERR_RESOLVE;

        return PREPC_UDP_ERR_INTERNAL;
    }

    if (ctx->addrlen > 0 && ctx->sock >= 0) {
        for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
            if (sockaddr_equal((struct sockaddr *)&ctx->addr, p->ai_addr)) {
                freeaddrinfo(res);
                return PREPC_UDP_ERR_OK;
            }
        }
    }

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        int sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0)
            continue;

        memcpy(&ctx->addr, p->ai_addr, p->ai_addrlen);
        ctx->addrlen = p->ai_addrlen;

        if (ctx->sock >= 0)
            close(ctx->sock);
        ctx->sock = sock;

        freeaddrinfo(res);
        return PREPC_UDP_ERR_OK_HOST_CHANGED;
    }

    freeaddrinfo(res);
    return PREPC_UDP_ERR_INTERNAL;
}

prepcUdpErr_t prepc_udp_reresolve(void *context) {
    connContext_t *ctx = (connContext_t *)context;

    return udp_reopen(ctx);
}

prepcUdpErr_t prepc_udp_init(void **context, const char *host, const char *port) {
    if (!context || !host || !port)
        return PREPC_UDP_ERR_INVALID_ARGUMENT;

    *context = NULL;

    if (strlen(host) >= PREPC_UDP_HOST_MAX_LEN)
        return PREPC_UDP_ERR_INVALID_ARGUMENT;

    if (strlen(port) >= PREPC_UDP_PORT_MAX_LEN)
        return PREPC_UDP_ERR_INVALID_ARGUMENT;

    connContext_t *ctx = calloc(1, sizeof(connContext_t));
    if (!ctx)
        return PREPC_UDP_ERR_INTERNAL;

    strcpy(ctx->host, host);
    strcpy(ctx->port, port);

    ctx->sock = -1;
    ctx->addrlen = 0;

    prepcUdpErr_t err = udp_reopen(ctx);
    if (err != PREPC_UDP_ERR_OK && err != PREPC_UDP_ERR_OK_HOST_CHANGED) {
        free(ctx);
        return err;
    }

    *context = ctx;
    return PREPC_UDP_ERR_OK;
}

prepcUdpErr_t prepc_udp_send(void *context, const uint8_t *data, size_t len) {
    connContext_t *ctx = (connContext_t *)context;

    if (!ctx || !data || ctx->sock < 0 || ctx->addrlen == 0)
        return PREPC_UDP_ERR_INVALID_ARGUMENT;

    ssize_t sent = sendto(ctx->sock, data, len, 0, (struct sockaddr *)&ctx->addr, ctx->addrlen);

    if (sent < 0) {
        if (errno == ENETUNREACH || errno == EHOSTUNREACH)
            return PREPC_UDP_ERR_NETWORK;

        if (errno == EMSGSIZE)
            return PREPC_UDP_ERR_INVALID_ARGUMENT;

        return PREPC_UDP_ERR_INTERNAL;
    }

    if ((size_t)sent != len)
        return PREPC_UDP_ERR_INTERNAL;

    return PREPC_UDP_ERR_OK;
}

void prepc_udp_cleanup(void *context) {
    connContext_t *ctx = (connContext_t *)context;

    if (!ctx)
        return;

    if (ctx->sock >= 0)
        close(ctx->sock);

    ctx->host[0] = '\0';
    ctx->port[0] = '\0';

    memset(&ctx->addr, 0, sizeof(ctx->addr));
    ctx->addrlen = 0;

    ctx->sock = -1;

    free(ctx);
}
