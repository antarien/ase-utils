#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <array>

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

        std::array<int, 256> decode_table{};
        std::fill(decode_table.begin(), decode_table.end(), -1);
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
        std::array<int, 256> table{};
        std::fill(table.begin(), table.end(), -1);
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
