#pragma once

/**
 * @file        strops.hpp
 * @brief       Safe C-string operations for ECS components and resource managers
 * @description Replaces forbidden C string functions (strncat, strrchr, strlen, snprintf)
 *              with safe, bounded helpers. All operations are null-termination guaranteed.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @created     2026-04-05
 * @modified    2026-04-05
 * @version     1.0.0
 */

#include <cstdint>

namespace ase::utils {

/** Copy src into dst with guaranteed null termination. dst_size includes null byte. */
inline void str_copy(char* dst, uint32_t dst_size, const char* src) {
    if (dst_size == 0) return;
    uint32_t i = 0;
    while (i < dst_size - 1 && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

/** Append src to dst with guaranteed null termination. dst_size is total buffer size. */
inline void str_append(char* dst, uint32_t dst_size, const char* src) {
    uint32_t di = 0;
    while (di < dst_size && dst[di] != '\0') ++di;
    uint32_t si = 0;
    while (di < dst_size - 1 && src[si] != '\0') {
        dst[di++] = src[si++];
    }
    if (di < dst_size) dst[di] = '\0';
}

/** Get string length (bounded by max_len to prevent unbounded scan). */
inline uint32_t str_len(const char* s, uint32_t max_len) {
    uint32_t i = 0;
    while (i < max_len && s[i] != '\0') ++i;
    return i;
}

/** Find last occurrence of ch in s. Returns index or -1 if not found. */
inline int32_t str_rfind(const char* s, uint32_t max_len, char ch) {
    int32_t last = -1;
    for (uint32_t i = 0; i < max_len && s[i] != '\0'; ++i) {
        if (s[i] == ch) last = static_cast<int32_t>(i);
    }
    return last;
}

/** Build path from segments: a/b/c/d. Null or empty segments are skipped. */
inline void str_path(char* out, uint32_t out_size, const char* a, const char* b,
                     const char* c, const char* d) {
    out[0] = '\0';
    if (a && a[0]) str_copy(out, out_size, a);
    if (b && b[0]) { str_append(out, out_size, "/"); str_append(out, out_size, b); }
    if (c && c[0]) { str_append(out, out_size, "/"); str_append(out, out_size, c); }
    if (d && d[0]) { str_append(out, out_size, "/"); str_append(out, out_size, d); }
}

/** Compare two strings up to max_len bytes. Returns true if equal. */
inline bool str_equal(const char* a, const char* b, uint32_t max_len) {
    for (uint32_t i = 0; i < max_len; ++i) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
    }
    return true;
}

/**
 * Format the low 24 bits of rgb as "#RRGGBB" into out.
 * out_size must be at least 8 (7 chars + null terminator). Higher bits are ignored.
 */
inline void format_hex_color(char* out, uint32_t out_size, uint32_t rgb) {
    if (out_size < 8) {
        if (out_size > 0) out[0] = '\0';
        return;
    }
    const char digits[] = "0123456789ABCDEF";
    out[0] = '#';
    out[1] = digits[(rgb >> 20) & 0xF];
    out[2] = digits[(rgb >> 16) & 0xF];
    out[3] = digits[(rgb >> 12) & 0xF];
    out[4] = digits[(rgb >>  8) & 0xF];
    out[5] = digits[(rgb >>  4) & 0xF];
    out[6] = digits[(rgb      ) & 0xF];
    out[7] = '\0';
}

/**
 * Format three 0..255 channel values as "#RRGGBB" into out.
 * out_size must be at least 8. Channels above 255 are clamped.
 */
inline void format_hex_rgb(char* out, uint32_t out_size,
                           uint32_t r, uint32_t g, uint32_t b) {
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    format_hex_color(out, out_size, (r << 16) | (g << 8) | b);
}

}  // namespace ase::utils
