#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include "udpHal.h"

typedef struct {
    SOCKET sock;

    char host[HOST_MAX_LEN];
    char port[PORT_MAX_LEN];

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

static halUdpErr_t udp_reopen(connContext_t *ctx) {
    if (!ctx)
        return HAL_UDP_ERR_INVALID_ARGUMENT;

    struct addrinfo hints = {0};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    struct addrinfo *res = NULL;

    int err = getaddrinfo(ctx->host, ctx->port, &hints, &res);
    if (err != 0) {
        if (err == EAI_AGAIN)
            return HAL_UDP_ERR_NETWORK;

        if (err == EAI_NONAME)
            return HAL_UDP_ERR_RESOLVE;

        return HAL_UDP_ERR_INTERNAL;
    }

    if (ctx->addrlen > 0 && ctx->sock != INVALID_SOCKET) {
        for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
            if (sockaddr_equal((struct sockaddr *)&ctx->addr, p->ai_addr)) {
                freeaddrinfo(res);
                return HAL_UDP_ERR_OK;
            }
        }
    }

    for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
        SOCKET sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET)
            continue;

        memcpy(&ctx->addr, p->ai_addr, p->ai_addrlen);
        ctx->addrlen = p->ai_addrlen;

        if (ctx->sock != INVALID_SOCKET)
            closesocket(ctx->sock);
        ctx->sock = sock;

        freeaddrinfo(res);
        return HAL_UDP_ERR_OK_HOST_CHANGED;
    }

    freeaddrinfo(res);
    return HAL_UDP_ERR_INTERNAL;
}

halUdpErr_t hal_udp_reresolve(void *context) {
    connContext_t *ctx = (connContext_t *)context;

    return udp_reopen(ctx);
}

halUdpErr_t hal_udp_init(void **context, const char *host, const char *port) {
    if (!context || !host || !port)
        return HAL_UDP_ERR_INVALID_ARGUMENT;

    *context = NULL;

    if (strlen(host) >= HOST_MAX_LEN)
        return HAL_UDP_ERR_INVALID_ARGUMENT;

    if (strlen(port) >= PORT_MAX_LEN)
        return HAL_UDP_ERR_INVALID_ARGUMENT;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return HAL_UDP_ERR_INTERNAL;

    connContext_t *ctx = calloc(1, sizeof(connContext_t));
    if (!ctx) {
        WSACleanup();
        return HAL_UDP_ERR_INTERNAL;
    }

    strcpy(ctx->host, host);
    strcpy(ctx->port, port);

    ctx->sock = INVALID_SOCKET;
    ctx->addrlen = 0;

    halUdpErr_t err = udp_reopen(ctx);
    if (err != HAL_UDP_ERR_OK && err != HAL_UDP_ERR_OK_HOST_CHANGED) {
        WSACleanup();
        free(ctx);
        return err;
    }

    *context = ctx;
    return HAL_UDP_ERR_OK;
}

halUdpErr_t hal_udp_send_vector(void *context, const halUdpBuf_t *buffers, size_t count) {
    connContext_t *ctx = (connContext_t *)context;

    if (!ctx || !buffers || count == 0 || ctx->sock == INVALID_SOCKET || ctx->addrlen == 0)
        return HAL_UDP_ERR_INVALID_ARGUMENT;

    if (count > HAL_UDP_MAX_BUFFERS)
        return HAL_UDP_ERR_INVALID_ARGUMENT;

    WSABUF wsabuf[HAL_UDP_MAX_BUFFERS];

    size_t total = 0;

    for (size_t i = 0; i < count; ++i) {
        if (!buffers[i].data && buffers[i].len != 0)
            return HAL_UDP_ERR_INVALID_ARGUMENT;

        if (buffers[i].len > ULONG_MAX)
            return HAL_UDP_ERR_INVALID_ARGUMENT;

        wsabuf[i].buf = (CHAR *)buffers[i].data;
        wsabuf[i].len = (ULONG)buffers[i].len;

        total += buffers[i].len;
    }

    DWORD sent = 0;

    int ret = WSASendTo(ctx->sock, wsabuf, (DWORD)count, &sent, 0, (struct sockaddr *)&ctx->addr,
                        (int)ctx->addrlen, NULL, NULL);

    if (ret == SOCKET_ERROR) {
        switch (WSAGetLastError()) {
            case WSAENETUNREACH:
            case WSAEHOSTUNREACH:
                return HAL_UDP_ERR_NETWORK;

            case WSAEMSGSIZE:
                return HAL_UDP_ERR_INVALID_ARGUMENT;

            default:
                return HAL_UDP_ERR_INTERNAL;
        }
    }

    if ((size_t)sent != total)
        return HAL_UDP_ERR_INTERNAL;

    return HAL_UDP_ERR_OK;
}

halUdpErr_t hal_udp_send(void *context, const uint8_t *data, size_t len) {
    halUdpBuf_t buf = {.data = data, .len = len};

    return hal_udp_send_vector(context, &buf, 1);
}

void hal_udp_cleanup(void *context) {
    connContext_t *ctx = (connContext_t *)context;

    if (!ctx)
        return;

    if (ctx->sock != INVALID_SOCKET)
        closesocket(ctx->sock);

    ctx->host[0] = '\0';
    ctx->port[0] = '\0';

    memset(&ctx->addr, 0, sizeof(ctx->addr));
    ctx->addrlen = 0;

    ctx->sock = INVALID_SOCKET;

    free(ctx);

    WSACleanup();
}
