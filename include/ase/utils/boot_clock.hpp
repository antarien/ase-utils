#pragma once

/**
 * ASE UTILS - BOOT CLOCK
 *
 * @file        boot_clock.hpp
 * @brief       Elapsed real time that keeps counting while the system is suspended
 * @description The third clock beside the two of clock.hpp, for exactly one purpose: a DURATION
 *              that spans a suspend of the whole system - the absence of an application whose
 *              device slept. Header-only like clock.hpp, built on the same C call, and like every
 *              foundation header it binds no ase:: target.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    time/clock
 * @created     2026-10-07
 * @modified    2026-10-07
 * @version     1.0.0
 */

#include <ctime>
#include <cstdint>

namespace ase::utils {

/*
 * THE CLOCK THAT DOES NOT STOP WHILE THE SYSTEM SLEEPS (2026-10-07).
 *
 * monotonic_nanos() of clock.hpp reads CLOCK_MONOTONIC, and CLOCK_MONOTONIC counts no time in
 * which the system is suspended. A process suspended together with its device - an application in
 * the pocket of a sleeping handset - measures across that sleep only the time the device was
 * awake. For a host that hands an application's absence to a simulation as elapsed real time
 * (KernelEmbeddedHost::advance, ARCH_ASE_KERNEL_EMBEDDED "Zeit ohne Pause") that is the wrong
 * span: the world would lose every hour the device slept. Godot's own ticks carry the same blind
 * spot, measured in its source: OS_Unix::get_ticks_usec reads CLOCK_MONOTONIC_RAW
 * (drivers/unix/os_unix.cpp, 4.5.2-stable), and OS_Android inherits it unchanged.
 *
 * CLOCK_BOOTTIME is CLOCK_MONOTONIC plus the time spent in suspend. Everything clock.hpp says
 * about monotonic_nanos() holds here as well - it only moves forward, nobody can set it, and it
 * carries no date, so a value from it means nothing across a reboot. The one difference is that a
 * suspend is counted. It is still not the wall clock: a user who sets the date moves
 * wall_time_seconds(), never this.
 *
 * WHY A HEADER OF ITS OWN AND NOT A LINE IN clock.hpp: clock.hpp is read across the tree
 * (ase-ecs, ase-log, ase-fileio, ase-network, ase-replication), and this clock has one consumer
 * circle - an application that embeds the host and is suspended by its operating system. A line
 * added there would recompile every one of those units for a purpose none of them has.
 *
 *   duration across a suspend of the system (an app's absence)   → boot_nanos()
 *   duration within one waking span (frame, tick, timeout)        → monotonic_nanos()
 *   deadline, expiry, timestamp, date                             → wall_time_seconds()
 *
 * Nanoseconds like monotonic_nanos(), so the divisors of clock.hpp (NANOS_PER_SECOND and its
 * siblings) apply unchanged.
 */
inline int64_t boot_nanos() {
    struct timespec ts {};
    ::clock_gettime(CLOCK_BOOTTIME, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000000000LL + static_cast<int64_t>(ts.tv_nsec);
}

}  // namespace ase::utils
