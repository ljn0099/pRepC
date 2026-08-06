/*
 * Copyright (C) 2026 by ljn0099
 *
 * Permission to use, copy, modify, and/or distribute this software for any purpose with or without
 * fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/* Portability notes:
 * Assumes 8-bit bytes
 * Assumes two's complement representation for signed integers.
 * Relies on fixed-width integer types (stdint.h)
 */

#ifndef BINIO_H
#define BINIO_H

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define BINIO_VERSION_MAJOR 1
#define BINIO_VERSION_MINOR 1
#define BINIO_VERSION_PATCH 0

#if (defined(BINIO_LITTLE_ENDIAN) && defined(BINIO_BIG_ENDIAN))
#error "Only one of BINIO_LITTLE_ENDIAN, BINIO_BIG_ENDIAN can be defined"
#endif

#if CHAR_BIT != 8
#error "BINIO requires CHAR_BIT == 8 (unsupported platform)"
#endif

// ----- 64 bits -----
static inline size_t binio_write_u64_le(uint8_t *buf, uint64_t v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, sizeof(uint64_t));
#else
    buf[0] = (uint8_t)(v >> 0);
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 24);
    buf[4] = (uint8_t)(v >> 32);
    buf[5] = (uint8_t)(v >> 40);
    buf[6] = (uint8_t)(v >> 48);
    buf[7] = (uint8_t)(v >> 56);
#endif
    return sizeof(uint64_t);
}

static inline size_t binio_write_i64_le(uint8_t *buf, int64_t v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, sizeof(int64_t));
#else
    uint64_t u = (uint64_t)v;
    buf[0] = (uint8_t)(u >> 0);
    buf[1] = (uint8_t)(u >> 8);
    buf[2] = (uint8_t)(u >> 16);
    buf[3] = (uint8_t)(u >> 24);
    buf[4] = (uint8_t)(u >> 32);
    buf[5] = (uint8_t)(u >> 40);
    buf[6] = (uint8_t)(u >> 48);
    buf[7] = (uint8_t)(u >> 56);
#endif
    return sizeof(int64_t);
}

static inline size_t binio_read_u64_le(const uint8_t *buf, uint64_t *v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(v, buf, sizeof(uint64_t));
#else
    *v = ((uint64_t)buf[0] << 0) | ((uint64_t)buf[1] << 8) | ((uint64_t)buf[2] << 16) |
         ((uint64_t)buf[3] << 24) | ((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40) |
         ((uint64_t)buf[6] << 48) | ((uint64_t)buf[7] << 56);
#endif
    return sizeof(uint64_t);
}

static inline size_t binio_read_i64_le(const uint8_t *buf, int64_t *v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(v, buf, sizeof(int64_t));
#else
    uint64_t u = ((uint64_t)buf[0] << 0) | ((uint64_t)buf[1] << 8) | ((uint64_t)buf[2] << 16) |
                 ((uint64_t)buf[3] << 24) | ((uint64_t)buf[4] << 32) | ((uint64_t)buf[5] << 40) |
                 ((uint64_t)buf[6] << 48) | ((uint64_t)buf[7] << 56);

    *v = (int64_t)u;
#endif
    return sizeof(int64_t);
}

static inline size_t binio_write_u64_be(uint8_t *buf, uint64_t v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, &v, sizeof(uint64_t));
#else
    buf[0] = (uint8_t)(v >> 56);
    buf[1] = (uint8_t)(v >> 48);
    buf[2] = (uint8_t)(v >> 40);
    buf[3] = (uint8_t)(v >> 32);
    buf[4] = (uint8_t)(v >> 24);
    buf[5] = (uint8_t)(v >> 16);
    buf[6] = (uint8_t)(v >> 8);
    buf[7] = (uint8_t)(v >> 0);
#endif
    return sizeof(uint64_t);
}

static inline size_t binio_write_i64_be(uint8_t *buf, int64_t v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, &v, sizeof(int64_t));
#else
    uint64_t u = (uint64_t)v;
    buf[0] = (uint8_t)(u >> 56);
    buf[1] = (uint8_t)(u >> 48);
    buf[2] = (uint8_t)(u >> 40);
    buf[3] = (uint8_t)(u >> 32);
    buf[4] = (uint8_t)(u >> 24);
    buf[5] = (uint8_t)(u >> 16);
    buf[6] = (uint8_t)(u >> 8);
    buf[7] = (uint8_t)(u >> 0);
#endif
    return sizeof(int64_t);
}

static inline size_t binio_read_u64_be(const uint8_t *buf, uint64_t *v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(v, buf, sizeof(uint64_t));
#else
    *v = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) | ((uint64_t)buf[2] << 40) |
         ((uint64_t)buf[3] << 32) | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
         ((uint64_t)buf[6] << 8) | ((uint64_t)buf[7] << 0);
#endif
    return sizeof(uint64_t);
}

static inline size_t binio_read_i64_be(const uint8_t *buf, int64_t *v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(v, buf, sizeof(int64_t));
#else
    uint64_t u = ((uint64_t)buf[0] << 56) | ((uint64_t)buf[1] << 48) | ((uint64_t)buf[2] << 40) |
                 ((uint64_t)buf[3] << 32) | ((uint64_t)buf[4] << 24) | ((uint64_t)buf[5] << 16) |
                 ((uint64_t)buf[6] << 8) | ((uint64_t)buf[7] << 0);

    *v = (int64_t)u;
#endif
    return sizeof(int64_t);
}

// ----- 32 bits -----
static inline size_t binio_write_u32_le(uint8_t *buf, uint32_t v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, sizeof(uint32_t));
#else
    buf[0] = (uint8_t)(v);
    buf[1] = (uint8_t)(v >> 8);
    buf[2] = (uint8_t)(v >> 16);
    buf[3] = (uint8_t)(v >> 24);
#endif
    return sizeof(uint32_t);
}

static inline size_t binio_write_i32_le(uint8_t *buf, int32_t v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, sizeof(int32_t));
#else
    uint32_t u = (uint32_t)v;
    buf[0] = (uint8_t)(u);
    buf[1] = (uint8_t)(u >> 8);
    buf[2] = (uint8_t)(u >> 16);
    buf[3] = (uint8_t)(u >> 24);
#endif
    return sizeof(int32_t);
}

static inline size_t binio_read_u32_le(const uint8_t *buf, uint32_t *v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(v, buf, sizeof(uint32_t));
#else
    *v = ((uint32_t)buf[0] << 0) | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
         ((uint32_t)buf[3] << 24);
#endif
    return sizeof(uint32_t);
}

static inline size_t binio_read_i32_le(const uint8_t *buf, int32_t *v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(v, buf, sizeof(int32_t));
#else
    uint32_t u = ((uint32_t)buf[0] << 0) | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
                 ((uint32_t)buf[3] << 24);
    *v = (int32_t)u;
#endif
    return sizeof(int32_t);
}

static inline size_t binio_write_u32_be(uint8_t *buf, uint32_t v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, &v, sizeof(uint32_t));
#else
    buf[0] = (uint8_t)(v >> 24);
    buf[1] = (uint8_t)(v >> 16);
    buf[2] = (uint8_t)(v >> 8);
    buf[3] = (uint8_t)(v >> 0);
#endif
    return sizeof(uint32_t);
}

static inline size_t binio_write_i32_be(uint8_t *buf, int32_t v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, &v, sizeof(int32_t));
#else
    uint32_t u = (uint32_t)v;
    buf[0] = (uint8_t)(u >> 24);
    buf[1] = (uint8_t)(u >> 16);
    buf[2] = (uint8_t)(u >> 8);
    buf[3] = (uint8_t)(u >> 0);
#endif
    return sizeof(int32_t);
}

static inline size_t binio_read_u32_be(const uint8_t *buf, uint32_t *v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(v, buf, sizeof(uint32_t));
#else
    *v = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) |
         ((uint32_t)buf[3] << 0);
#endif
    return sizeof(uint32_t);
}

static inline size_t binio_read_i32_be(const uint8_t *buf, int32_t *v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(v, buf, sizeof(int32_t));
#else
    uint32_t u = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) |
                 ((uint32_t)buf[3] << 0);
    *v = (int32_t)u;
#endif
    return sizeof(int32_t);
}

// ----- 16 bits -----
static inline size_t binio_write_u16_le(uint8_t *buf, uint16_t v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, sizeof(uint16_t));
#else
    buf[0] = (uint8_t)(v);
    buf[1] = (uint8_t)(v >> 8);
#endif
    return sizeof(uint16_t);
}

static inline size_t binio_write_i16_le(uint8_t *buf, int16_t v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, sizeof(int16_t));
#else
    uint16_t u = (uint16_t)v;
    buf[0] = (uint8_t)(u);
    buf[1] = (uint8_t)(u >> 8);
#endif
    return sizeof(int16_t);
}

static inline size_t binio_read_u16_le(const uint8_t *buf, uint16_t *v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(v, buf, sizeof(uint16_t));
#else
    *v = ((uint16_t)buf[0] << 0) | ((uint16_t)buf[1] << 8);
#endif
    return sizeof(uint16_t);
}

static inline size_t binio_read_i16_le(const uint8_t *buf, int16_t *v) {
#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(v, buf, sizeof(int16_t));
#else
    uint16_t u = ((uint16_t)buf[0] << 0) | ((uint16_t)buf[1] << 8);
    *v = (int16_t)u;
#endif
    return sizeof(int16_t);
}

static inline size_t binio_write_u16_be(uint8_t *buf, uint16_t v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, &v, sizeof(uint16_t));
#else
    buf[0] = (uint8_t)(v >> 8);
    buf[1] = (uint8_t)(v >> 0);
#endif
    return sizeof(uint16_t);
}

static inline size_t binio_write_i16_be(uint8_t *buf, int16_t v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, &v, sizeof(int16_t));
#else
    uint16_t u = (uint16_t)v;
    buf[0] = (uint8_t)(u >> 8);
    buf[1] = (uint8_t)(u >> 0);
#endif
    return sizeof(int16_t);
}

static inline size_t binio_read_u16_be(const uint8_t *buf, uint16_t *v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(v, buf, sizeof(uint16_t));
#else
    *v = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1] << 0);
#endif
    return sizeof(uint16_t);
}

static inline size_t binio_read_i16_be(const uint8_t *buf, int16_t *v) {
#if defined(BINIO_BIG_ENDIAN)
    memcpy(v, buf, sizeof(int16_t));
#else
    uint16_t u = ((uint16_t)buf[0] << 8) | ((uint16_t)buf[1] << 0);
    *v = (int16_t)u;
#endif
    return sizeof(int16_t);
}

// ----- 8 bits -----
static inline size_t binio_write_u8(uint8_t *buf, uint8_t v) {
    buf[0] = v;
    return sizeof(uint8_t);
}

static inline size_t binio_write_i8(uint8_t *buf, int8_t v) {
    buf[0] = (uint8_t)v;
    return sizeof(int8_t);
}

static inline size_t binio_read_u8(const uint8_t *buf, uint8_t *v) {
    *v = buf[0];
    return sizeof(uint8_t);
}

static inline size_t binio_read_i8(const uint8_t *buf, int8_t *v) {
    *v = (int8_t)buf[0];
    return sizeof(int8_t);
}

// ----- Bytes Stream -----
static inline size_t binio_write_bytes(uint8_t *buf, const uint8_t *src, size_t len) {
    memcpy(buf, src, len);
    return len;
}

static inline size_t binio_read_bytes(const uint8_t *buf, uint8_t *dst, size_t len) {
    memcpy(dst, buf, len);
    return len;
}

// ----- Skip -----
static inline size_t binio_skip(size_t n) {
    return n;
}

static inline size_t binio_skip_zero(uint8_t *buf, size_t n) {
    memset(buf, 0, n);

    return n;
}

// ----- Number of LSB -----
static inline size_t binio_write_u64_lsb_le(uint8_t *buf, uint64_t v, size_t nBytes) {
    if (nBytes > sizeof(uint64_t))
        nBytes = sizeof(uint64_t);

#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, nBytes);
#else
    for (size_t i = 0; i < nBytes; i++)
        buf[i] = (uint8_t)(v >> (i * 8));
#endif

    return nBytes;
}

static inline size_t binio_write_u64_lsb_be(uint8_t *buf, uint64_t v, size_t nBytes) {
    if (nBytes > sizeof(uint64_t))
        nBytes = sizeof(uint64_t);

#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, ((const uint8_t *)&v) + (sizeof(uint64_t) - nBytes), nBytes);
#else
    size_t shift = (nBytes - 1) * 8;

    for (size_t i = 0; i < nBytes; ++i) {
        buf[i] = (uint8_t)(v >> shift);
        shift -= 8;
    }
#endif

    return nBytes;
}

static inline size_t binio_write_i64_lsb_le(uint8_t *buf, int64_t v, size_t nBytes) {
    if (nBytes > sizeof(int64_t))
        nBytes = sizeof(int64_t);

#if defined(BINIO_LITTLE_ENDIAN)
    memcpy(buf, &v, nBytes);
#else
    uint64_t u = (uint64_t)v;

    for (size_t i = 0; i < nBytes; ++i)
        buf[i] = (uint8_t)(u >> (i * 8));
#endif

    return nBytes;
}

static inline size_t binio_write_i64_lsb_be(uint8_t *buf, int64_t v, size_t nBytes) {
    if (nBytes > sizeof(int64_t))
        nBytes = sizeof(int64_t);

#if defined(BINIO_BIG_ENDIAN)
    memcpy(buf, ((const uint8_t *)&v) + (sizeof(int64_t) - nBytes), nBytes);
#else
    uint64_t u = (uint64_t)v;
    size_t shift = (nBytes - 1) * 8;

    for (size_t i = 0; i < nBytes; ++i) {
        buf[i] = (uint8_t)(u >> shift);
        shift -= 8;
    }
#endif

    return nBytes;
}

#endif
