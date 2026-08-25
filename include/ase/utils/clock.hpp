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
 * @category    time/clock
 * @created     2026-06-10
 * @modified    2026-08-20
 * @version     1.0.0
 */

#include <ctime>
#include <cstdint>

namespace ase::utils {

// Seconds since the Unix epoch (UTC). Coarse 1-second resolution — suitable for
// rate-limit windows, expiry checks and telemetry; NOT a high-resolution timer.
inline int64_t wall_time_seconds() {
    return static_cast<int64_t>(::time(nullptr));
}

/*
 * THE SAME CLOCK, FINER — FOR A TIMESTAMP SOMEBODY READS (2026-08-20).
 *
 * wall_time_seconds() cannot serve a printed timestamp of the form 14:57:03.412: its
 * resolution is the whole second, and the missing three digits are exactly what makes two
 * boot lines distinguishable. Measured in ase-ecs: terminal_utils.cpp builds both of its
 * timestamps as std::chrono::system_clock plus a millisecond remainder, which is 9 of that
 * module's findings and has no other reachable source.
 *
 * DO NOT ASSEMBLE THIS FROM THE OTHER TWO. Taking the seconds from wall_time_seconds() and
 * the sub-second part from monotonic_nanos() reads TWO clocks that are not related: the
 * fraction would not belong to the second it is printed next to, and it would drift apart
 * over a run. One clock, read once.
 *
 * It is still the wall clock, so everything wall_time_seconds() warns about holds here:
 * somebody can set it, and a duration measured with it can come out negative.
 */
inline int64_t wall_time_millis() {
    struct timespec ts {};
    ::clock_gettime(CLOCK_REALTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000LL +
           static_cast<int64_t>(ts.tv_nsec) / 1000000LL;
}

/*
 * THE SECOND CLOCK, AND IT IS NOT THE WALL CLOCK (2026-08-20).
 *
 * STD_CHRONO_FORBIDDEN asks the reader to decide between WALL-CLOCK and SIMULATION time. A
 * measurement across the tree found that the majority wants NEITHER: of 50 clock uses, 5 are
 * std::chrono::system_clock (wall clock, served by wall_time_seconds above) and 45 are
 * steady_clock or high_resolution_clock — code measuring a DURATION.
 *
 * A duration must not be measured with the wall clock, for exactly the reason the rule gives
 * for using it on deadlines: a wall clock can be SET. An NTP correction between two reads
 * yields a negative or absurd span, silently, and no gate reports it. This source is monotonic
 * — it only moves forward, nobody can set it, and it carries no date. That also means it is
 * useless for an expiry: a value from it is meaningless across process restarts.
 *
 *   duration, timeout, frame or tick time  → monotonic_nanos()
 *   deadline, expiry, timestamp, date      → wall_time_seconds()
 *
 * Nanoseconds is the base unit because the tree needs all four resolutions (measured:
 * 5 nanoseconds, 3 microseconds, 6 milliseconds, 2 seconds). One source plus a named divisor
 * beats four functions that would each be this same call divided by a constant.
 */
inline int64_t monotonic_nanos() {
    struct timespec ts {};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + static_cast<int64_t>(ts.tv_nsec);
}

constexpr int64_t NANOS_PER_MICRO  = 1000LL;        // monotonic_nanos() / NANOS_PER_MICRO
constexpr int64_t NANOS_PER_MILLI  = 1000000LL;     // monotonic_nanos() / NANOS_PER_MILLI
constexpr int64_t NANOS_PER_SECOND = 1000000000LL;  // monotonic_nanos() / NANOS_PER_SECOND
constexpr int64_t MILLIS_PER_SECOND = 1000LL;       // wall_time_millis() split into s and ms

}  // namespace ase::utils
