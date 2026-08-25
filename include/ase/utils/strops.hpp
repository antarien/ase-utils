#pragma once

/**
 * @file        strops.hpp
 * @brief       Safe C-string operations for ECS components and resource managers
 * @description Replaces forbidden C string functions (strncat, strrchr, strlen, snprintf)
 *              with safe, bounded helpers. All operations are null-termination guaranteed.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    structure/datatype/textual
 * @created     2026-04-05
 * @modified    2026-08-20
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

/**
 * Append src to dst with guaranteed null termination. dst_size is total buffer size.
 *
 * The dst_size == 0 guard closes a hole, it does not change behaviour: without it the
 * `dst_size - 1` below wraps to UINT32_MAX, the loop condition holds, and a byte is written
 * into dst[0] — a buffer the caller declared as empty. That was never defined behaviour, so
 * no caller could have relied on it. str_copy and str_append_u64 carry the same guard; this
 * one was the odd sibling out.
 */
inline void str_append(char* dst, uint32_t dst_size, const char* src) {
    if (dst_size == 0) return;
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
 * Append an unsigned decimal to dst — the bounded replacement for snprintf("%llu").
 *
 * Emits most-significant digit first (scale is the largest power of ten not
 * exceeding v), so no reversal buffer is needed and zero renders as "0" without
 * a special case. Truncates rather than overflowing, exactly like str_append.
 */
inline void str_append_u64(char* dst, uint32_t dst_size, uint64_t v) {
    if (dst_size == 0) return;
    uint32_t di = 0;
    while (di < dst_size && dst[di] != '\0') ++di;
    uint64_t scale = 1;
    while (v / scale >= 10u) scale *= 10u;
    while (di < dst_size - 1) {
        dst[di++] = static_cast<char>('0' + static_cast<uint32_t>((v / scale) % 10u));
        if (scale == 1) break;
        scale /= 10u;
    }
    if (di < dst_size) dst[di] = '\0';
}

/**
 * Append a SIGNED decimal to dst — the bounded replacement for snprintf("%lld").
 *
 * This exists because str_append_u64 takes uint64: handing it a negative int32 sign-extends
 * it, and a value of -3 leaves as 18446744073709551613. That is not a crash but a plausible
 * wrong number, which is worse — a line whose whole statement is "this address resolves to
 * nothing" would then read "address 18446744071562067968" (GEOID_INVALID_CELL_AXIS is
 * INT32_MIN, and the log lines that report it are exactly the ones that print the sentinel).
 *
 * The magnitude is formed over uint64, NOT over -v: negating INT64_MIN is undefined.
 * ~u + 1 is the two's complement and yields exactly 9223372036854775808 for INT64_MIN.
 *
 * The dst_size == 0 guard is NOT redundant with the callees: str_append above has none, and
 * with a zero size its `dst_size - 1` wraps to UINT32_MAX and the loop writes into dst[0].
 */
inline void str_append_i64(char* dst, uint32_t dst_size, int64_t v) {
    if (dst_size == 0) return;
    if (v < 0) {
        str_append(dst, dst_size, "-");
        str_append_u64(dst, dst_size, ~static_cast<uint64_t>(v) + 1u);
        return;
    }
    str_append_u64(dst, dst_size, static_cast<uint64_t>(v));
}

/**
 * Append a float with a FIXED number of decimals — the bounded replacement for
 * snprintf("%.*f"). No locale, no printf, no exponent form.
 *
 * Three cases leave as a WORD instead of digits, because printing a number for a value that
 * is not one hides exactly the defect one most wants to see:
 *   nan   the value is not a number
 *   inf   the value is infinite
 *   huge  the value is FINITE but beyond uint64 and therefore out of reach here. Deliberately
 *         NOT "inf": calling a finite number infinite is the same class of false statement
 *         this function exists to prevent. The bound has to be there in any case — the
 *         uint64 cast below would be undefined without it.
 *
 * decimals is capped at 9 for two independent reasons that meet at the same number: 10^20
 * overflows the uint64 scale (a silent wrap, not a crash), and a float carries about seven
 * significant digits, so anything beyond that would be invented precision.
 */
inline void str_append_f32(char* dst, uint32_t dst_size, float v, uint32_t decimals) {
    if (dst_size == 0) return;
    if (v != v) {  // NaN is the only value unequal to itself
        str_append(dst, dst_size, "nan");
        return;
    }
    if (v < 0.0f) {
        str_append(dst, dst_size, "-");
        v = -v;
    }
    if (!(v <= 340282346638528859811704183484516925440.0f)) {  // FLT_MAX; above it means inf
        str_append(dst, dst_size, "inf");
        return;
    }
    if (!(v < 18446744073709551616.0f)) {  // finite, but past what uint64 can hold
        str_append(dst, dst_size, "huge");
        return;
    }
    if (decimals > 9u) decimals = 9u;
    uint64_t scale = 1;
    for (uint32_t i = 0; i < decimals; ++i) scale *= 10u;
    uint64_t whole = static_cast<uint64_t>(v);
    uint64_t frac =
        static_cast<uint64_t>((v - static_cast<float>(whole)) * static_cast<float>(scale) + 0.5f);
    if (frac >= scale) {  // the rounding carried into the next integer
        whole += 1u;
        frac = 0u;
    }
    str_append_u64(dst, dst_size, whole);
    if (decimals == 0u) return;
    str_append(dst, dst_size, ".");
    uint64_t probe = scale / 10u;
    while (probe > frac && probe > 0u) {  // leading zeros of the fraction
        str_append(dst, dst_size, "0");
        probe /= 10u;
    }
    if (frac > 0u) str_append_u64(dst, dst_size, frac);
}

/**
 * Append src to dst, SKIPPING the bytes that would break a quoted JSON string:
 * the double quote, the backslash, and every ASCII control byte.
 *
 * This exists because documents are assembled from request data, and a single
 * quote inside a user-supplied path or name would close the string early and
 * let the caller write arbitrary fields into the document. Skipping rather than
 * escaping keeps the output length bounded by the input length, which is what
 * the fixed-size frame buffers downstream rely on.
 */
inline void str_append_json_safe(char* dst, uint32_t dst_size, const char* src) {
    if (dst_size == 0) return;
    uint32_t di = 0;
    while (di < dst_size && dst[di] != '\0') ++di;
    for (uint32_t si = 0; src[si] != '\0' && di < dst_size - 1; ++si) {
        const char c = src[si];
        if (c == '"' || c == '\\') continue;
        if (static_cast<unsigned char>(c) < 32u) continue;
        dst[di++] = c;
    }
    if (di < dst_size) dst[di] = '\0';
}

/** Copy src into dst with the JSON-breaking bytes skipped (see str_append_json_safe). */
inline void str_copy_json_safe(char* dst, uint32_t dst_size, const char* src) {
    if (dst_size == 0) return;
    dst[0] = '\0';
    str_append_json_safe(dst, dst_size, src);
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

/**
 * Take the next field of `text` up to the next `separator`, and advance `pos` past it.
 *
 *     uint32_t pos = 0;
 *     const char* tok = nullptr;
 *     uint32_t tok_len = 0;
 *     while (str_split_next(text, len, ',', pos, &tok, &tok_len)) {
 *         // tok points into text and is tok_len characters long — no copy, no allocation
 *     }
 *
 * WHY THIS IS HERE AND NOT IN strmatch.hpp. Measured 2026-08-20 in core/ase-codegen: seven call
 * sites ran a full regex engine to split a list at commas — std::sregex_token_iterator with -1
 * over the pattern "\s*,\s*", six times built locally under two different names. Splitting at a
 * fixed character is a string operation, not a pattern search; it belongs with str_copy and
 * str_equal, and it needs no matcher at all. The surrounding whitespace those patterns also ate
 * is left in place deliberately: every one of the seven call sites already trims its token, and
 * trimming here would change what they receive.
 *
 * The field is NOT trimmed and NOT null-terminated — it is a view into `text`. A caller that
 * needs a C string copies it out with str_copy.
 *
 * @return true while a field was produced. An empty field between two separators is a field and
 *         is reported as one (length 0); the caller decides whether to skip it.
 */
constexpr bool str_split_next(const char* text, uint32_t text_len, char separator,
                              uint32_t& pos, const char** field, uint32_t* field_len) {
    if (text == nullptr || field == nullptr || field_len == nullptr) return false;
    if (pos > text_len) return false;

    const uint32_t begin = pos;
    uint32_t end = pos;
    while (end < text_len && text[end] != separator) {
        ++end;
    }

    *field = text + begin;
    *field_len = end - begin;

    // THE ADVANCE. Past the separator when there is one, past the end otherwise — and `text_len
    // + 1` is what makes the next call return false instead of handing out the same empty tail
    // forever. A trailing separator therefore yields one final empty field and then stops, which
    // is what "a,b," means: three fields, the last one empty.
    pos = (end < text_len) ? end + 1 : text_len + 1;
    return true;
}

}  // namespace ase::utils
