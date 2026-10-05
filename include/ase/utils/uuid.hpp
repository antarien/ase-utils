#pragma once

/**
 * ASE UTILS - UUID VERSION 7
 *
 * @file        uuid.hpp
 * @brief       UUIDv7 byte layout (RFC 9562) — Foundation SSOT
 * @description The tree carries UUIDv7 identities as 16 binary bytes (skill ids, project ids, the
 *              request_uuid of an authoring run on frames 56/57) and parses them from text in
 *              several places, but generated none until 2026-10-04. This is the layout only, and it
 *              is PURE: the caller hands in the millisecond time and the ten random bytes. The time
 *              comes from ase::utils::wall_time_millis() (std::chrono is forbidden tree-wide), the
 *              randomness from a source the caller owns - ase::crypto::secure_random_bytes() where
 *              an identity must not be guessable.
 *
 *              WHY THE TWO INPUTS ARE PARAMETERS. A function that drew them itself would give Layer
 *              0 its first ase:: dependency (OpenSSL behind ase-crypto) - the coupling dotenv.hpp
 *              dissolved instead of relocating on 2026-08-22, and the build file of this module
 *              says it outright: no dependencies in Layer 0. It would also hide two side effects
 *              behind one name. Here the caller reads, this header only lays out.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    structure/reference/identifier
 * @created     2026-10-04
 * @modified    2026-10-04
 * @version     1.0.0
 */

#include <cstdint>

namespace ase::utils {

constexpr uint32_t UUID_BYTES         = 16u;  // a UUID in binary form
constexpr uint32_t UUID_V7_RAND_BYTES = 10u;  // random bytes a UUIDv7 consumes (rand_a + rand_b)

/*
 * THE LAYOUT (RFC 9562, section 5.7), big-endian on the wire:
 *
 *   bytes 0..5   unix_ts_ms  - 48-bit milliseconds since the Unix epoch, so ids sort by time
 *   byte  6      0111 xxxx   - version 7 in the high nibble, 4 random bits below it
 *   byte  7      rand_a      - 8 more random bits
 *   byte  8      10xx xxxx   - variant 10 in the two high bits, 6 random bits below
 *   bytes 9..15  rand_b      - 56 more random bits
 *
 * The ten random bytes are spent in that order; the version and variant bits overwrite the high
 * bits of rnd[0] and rnd[2], so 74 of the 80 bits handed in end up in the id.
 */
inline void uuid_v7(char* out, int64_t unix_ms, const uint8_t* rnd) {
    const auto ms = static_cast<uint64_t>(unix_ms);
    for (uint32_t i = 0; i < 6u; ++i) {
        out[i] = static_cast<char>((ms >> ((5u - i) * 8u)) & 0xFFu);
    }
    out[6] = static_cast<char>(0x70u | (rnd[0] & 0x0Fu));
    out[7] = static_cast<char>(rnd[1]);
    out[8] = static_cast<char>(0x80u | (rnd[2] & 0x3Fu));
    for (uint32_t i = 9u; i < UUID_BYTES; ++i) {
        out[i] = static_cast<char>(rnd[i - 6u]);
    }
}

}  // namespace ase::utils
