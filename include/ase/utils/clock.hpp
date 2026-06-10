#pragma once

/**
 * ASE UTILS - WALL CLOCK
 *
 * @file        clock.hpp
 * @brief       Wall-clock time (Unix epoch seconds) — Foundation SSOT
 * @description std::chrono is validator-forbidden project-wide (rule STD_CHRONO_FORBIDDEN,
 *              file_filter null). This is the single sanctioned wall-clock primitive,
 *              built on C time(), mirroring the blessed
 *              AuthResourceManager::get_wall_time_seconds() pattern. Header-only so every
 *              layer can read coarse real time (rate-limit windows, token-expiry checks,
 *              telemetry) without a link dependency.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 */

#include <ctime>
#include <cstdint>

namespace ase::utils {

// Seconds since the Unix epoch (UTC). Coarse 1-second resolution — suitable for
// rate-limit windows, expiry checks and telemetry; NOT a high-resolution timer.
inline int64_t wall_time_seconds() {
    return static_cast<int64_t>(::time(nullptr));
}

}  // namespace ase::utils
