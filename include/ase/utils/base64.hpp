#pragma once

/**
 * @file        base64.hpp
 * @brief       Base64 encode/decode — standard and URL-safe alphabet
 * @description Three entry points: encode() and decode() over owning containers for callers
 *              that already hold one, and decode_url_into() as a ptr+len interface so an ECS
 *              system can decode a JWT segment into its own buffer without a std::vector.
 *
 *              THE DECODE TABLES ARE PLAIN C ARRAYS, NOT std::array (2026-08-20). Both are
 *              256 entries built on entry and read only inside the function that built them;
 *              std::array bought nothing here but the forbidden dependency, and the loop that
 *              fills them with -1 replaces a std::fill that pulled in <algorithm> as well.
 *              Neither table escapes its function, so there is no size to carry around.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    structure/format/encoding
 * @created     2025-12-16
 * @modified    2026-08-20
 * @version     1.0.0
 */

#include <string>
#include <vector>
#include <cstdint>

namespace ase::utils {

/**
 * Base64 encoding/decoding utilities
 */
struct Base64 {
    static constexpr const char* CHARS =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    static std::string encode(const std::vector<uint8_t>& data) {
        std::string result;
        result.reserve(((data.size() + 2) / 3) * 4);

        for (size_t i = 0; i < data.size(); i += 3) {
            uint32_t n = static_cast<uint32_t>(data[i]) << 16;
            if (i + 1 < data.size()) n |= static_cast<uint32_t>(data[i + 1]) << 8;
            if (i + 2 < data.size()) n |= static_cast<uint32_t>(data[i + 2]);

            result.push_back(CHARS[(n >> 18) & 0x3F]);
            result.push_back(CHARS[(n >> 12) & 0x3F]);
            result.push_back((i + 1 < data.size()) ? CHARS[(n >> 6) & 0x3F] : '=');
            result.push_back((i + 2 < data.size()) ? CHARS[n & 0x3F] : '=');
        }
        return result;
    }

    static std::vector<uint8_t> decode(const std::string& encoded) {
        std::vector<uint8_t> result;
        result.reserve((encoded.size() * 3) / 4);

        // -1 marks "not in the alphabet" - the loop below overwrites exactly the 64 valid
        // codepoints, everything else stays rejected.
        int decode_table[256];
        for (int i = 0; i < 256; ++i) decode_table[i] = -1;
        for (int i = 0; i < 64; ++i) {
            decode_table[static_cast<unsigned char>(CHARS[i])] = i;
        }

        uint32_t val = 0;
        int bits = 0;
        for (char c : encoded) {
            if (c == '=') break;
            int v = decode_table[static_cast<unsigned char>(c)];
            if (v < 0) continue;
            val = (val << 6) | static_cast<uint32_t>(v);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                result.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            }
        }
        return result;
    }

    // URL-safe decode (RFC 4648 section 5, no padding) into a caller buffer. Returns the decoded byte
    // count (capped at out_cap), or 0 on an invalid character. A ptr+len interface so ECS-system callers
    // (no std::vector) can decode a JWT segment in place. '-' maps to 62, '_' to 63; the standard '+'/'/'
    // alphabet is also accepted so a non-url-safe segment still decodes.
    static uint32_t decode_url_into(const char* in, uint32_t in_len, uint8_t* out, uint32_t out_cap) {
        int table[256];
        for (int i = 0; i < 256; ++i) table[i] = -1;
        for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(CHARS[i])] = i;
        table[static_cast<unsigned char>('-')] = 62;
        table[static_cast<unsigned char>('_')] = 63;

        uint32_t o = 0;
        uint32_t val = 0;
        int bits = 0;
        for (uint32_t i = 0; i < in_len; ++i) {
            char c = in[i];
            if (c == '=') break;
            int v = table[static_cast<unsigned char>(c)];
            if (v < 0) return 0;
            val = (val << 6) | static_cast<uint32_t>(v);
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                if (o < out_cap) out[o++] = static_cast<uint8_t>((val >> bits) & 0xFF);
            }
        }
        return o;
    }
};

}  // namespace ase::utils
