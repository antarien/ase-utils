#pragma once

/**
 * @file        fs.hpp
 * @brief       Filesystem operations - path manipulation, queries, directory listing
 * @description ASE-native filesystem API that replaces std::filesystem usage in client
 *              and module code. Internally wraps std::filesystem (allowed in foundation).
 *              Callers see only ASE-native types: ase::utils::fs::Path, DirEntry.
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    structure/filesystem
 * @created     2026-04-13
 * @modified    2026-08-20
 * @version     1.0.0
 */

#include <climits>
#include <unistd.h>

#include <filesystem>
#include <string>
#include <vector>

// No <string_view>: every path parameter here took std::string_view until 2026-08-20. The
// forbidden-type rule points at char* or entt::hashed_string, and neither fits - a hashed_string
// cannot be opened, and the SSOT the rule cites (WRFL_ASE_STRING_HANDLING.md, Section 9) lists raw
// char* under its own anti-patterns. const std::string& is what the bodies already built anyway:
// each one wrapped its view in std::string(path) before std::filesystem could use it, so the views
// bought a copy each and no caller ever saw the saving.
//
// MEASURED before the change, because 17 files and 44 call sites depend on these signatures:
// nobody passes a std::string_view to any fs:: function (positive control: 350 std::string hits in
// the same scope), and the only two string_view uses in all five consumer trees are in
// ase-convert/console.hpp, unrelated to this header. Zero callers break - and the direction is the
// safe one, since const std::string& still converts implicitly to string_view, never the reverse.

namespace ase::utils::fs {

/**
 * Path - thin wrapper around std::string with path-like operations.
 * Stores a normalized POSIX-style string. Implicit conversion to const std::string&
 * lets Path be passed directly to all fs:: query functions.
 *
 * The std::string_view constructor was deleted on 2026-08-20 along with the view parameters:
 * with Path(const char*) and Path(std::string) both present it had nothing left to accept.
 */
class Path {
public:
    Path() = default;
    explicit Path(const char* p) : m_path(p ? p : "") {}
    explicit Path(std::string p) : m_path(std::move(p)) {}

    Path operator/(const std::string& seg) const {
        if (m_path.empty()) return Path(seg);
        if (m_path.back() == '/') return Path(m_path + seg);
        return Path(m_path + "/" + seg);
    }

    const std::string& str() const noexcept { return m_path; }
    const char* c_str() const noexcept { return m_path.c_str(); }
    bool empty() const noexcept { return m_path.empty(); }

    operator const std::string&() const noexcept { return m_path; }

    bool operator==(const Path& other) const noexcept { return m_path == other.m_path; }
    bool operator!=(const Path& other) const noexcept { return m_path != other.m_path; }

    Path parent() const {
        std::filesystem::path p(m_path);
        return Path(p.parent_path().string());
    }

    std::string filename() const {
        std::filesystem::path p(m_path);
        return p.filename().string();
    }

    std::string stem() const {
        std::filesystem::path p(m_path);
        return p.stem().string();
    }

    std::string extension() const {
        std::filesystem::path p(m_path);
        return p.extension().string();
    }

private:
    std::string m_path;
};

/** True if a filesystem entry exists at path. */
inline bool exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(path), ec);
}

/** True if path exists and is a directory. */
inline bool is_directory(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(path), ec);
}

/** True if path exists and is a regular file. */
inline bool is_regular_file(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(path), ec);
}

/** Return the parent directory of path. "/a/b/c" -> "/a/b". */
inline std::string parent_of(const std::string& path) {
    std::filesystem::path p(path);
    return p.parent_path().string();
}

/**
 * Absolute path of the process's current working directory, or "" when it cannot be
 * determined - which happens when the directory the process stands in was deleted
 * underneath it.
 *
 * POSIX getcwd RATHER THAN <filesystem>, ALTHOUGH THIS HEADER STILL INCLUDES THE LATTER.
 * The include is a debt that has to come off eventually; every new function that leans on
 * it makes that removal larger, so new ones do not. Nothing about this call needs the
 * heavier machinery: getcwd is one syscall and the answer is a byte string.
 *
 * ADDED 2026-08-22 BECAUSE THE GAP HAD AN ADDRESS AND NO OWNER. core/ase-codegen's
 * cli/cli_helpers.hpp carried the last two standard-file-API calls in that module for
 * exactly this reason, with a comment naming this header as their home. A gap that is
 * reported and then left standing reads, two days later, exactly like a gap nobody noticed.
 */
inline std::string current_path() {
    char buffer[PATH_MAX] = {};
    if (::getcwd(buffer, sizeof(buffer)) == nullptr) {
        return {};
    }
    return std::string(buffer);
}

/**
 * True when path has no parent left to climb to - the filesystem root, or the empty path.
 *
 * THE QUESTION IS "DOES AN UPWARD WALK END HERE", NOT "IS THIS A DIRECTORY". A search that
 * climbs toward the root needs it as its stop condition, because parent_of("/") is "/" again:
 * without this check the loop keeps asking the same directory forever, and it does so
 * silently, since every call in it succeeds.
 */
inline bool is_root(const std::string& path) {
    return path.empty() || path == "/";
}

/** Return the filename component of path. "/a/b/c.txt" -> "c.txt". */
inline std::string filename_of(const std::string& path) {
    std::filesystem::path p(path);
    return p.filename().string();
}

/**
 * Create the directory at path together with every missing parent. Returns
 * true on success AND when the directory already existed (that is not an
 * error, it is the desired end state), false on error (permission denied,
 * a non-directory component in the way, etc.).
 *
 * This is the ASE-native stand-in for shelling out to `mkdir -p`: a path
 * assembled from request data reaches no shell here, so nothing inside it can
 * be interpreted as a command. Callers building a path from untrusted input
 * still owe their own containment check (rejecting ".." and friends) — that
 * guards against escaping the intended root, which is a separate concern from
 * command injection and is NOT covered by avoiding the shell.
 */
inline bool create_directories(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path), ec);
    return !ec;
}

/**
 * Remove the filesystem entry at path. Works for both regular files and
 * directories; directory removal is recursive. Returns true on success (or
 * when the entry did not exist), false on error (permission denied,
 * cross-device link, etc.).
 */
inline bool remove(const std::string& path) {
    std::error_code ec;
    std::filesystem::path p(path);
    if (std::filesystem::is_directory(p, ec)) {
        std::filesystem::remove_all(p, ec);
    } else {
        std::filesystem::remove(p, ec);
    }
    return !ec;
}

/** Return path relative to base. relative_to("/a/b/c", "/a") -> "b/c". */
inline std::string relative_to(const std::string& path, const std::string& base) {
    std::error_code ec;
    auto rel = std::filesystem::relative(
        std::filesystem::path(path),
        std::filesystem::path(base),
        ec);
    if (ec) return path;
    return rel.string();
}

/**
 * DirEntry - one item from a directory listing.
 * Contains both the leaf name and the full absolute path.
 */
struct DirEntry {
    std::string name;
    std::string full_path;
    bool is_directory = false;
};

/**
 * List immediate children of dir. Permission errors are skipped silently.
 * Returns an empty vector if dir does not exist or is not a directory.
 * Order is filesystem-defined (caller should sort if order matters).
 */
inline std::vector<DirEntry> list_directory(const std::string& dir) {
    std::vector<DirEntry> result;
    std::filesystem::path dir_path(dir);
    std::error_code ec;
    if (!std::filesystem::is_directory(dir_path, ec)) return result;

    auto opts = std::filesystem::directory_options::skip_permission_denied;
    std::filesystem::directory_iterator it(dir_path, opts, ec);
    if (ec) return result;

    for (const auto& e : it) {
        DirEntry de;
        de.name = e.path().filename().string();
        if (de.name.empty()) continue;
        de.full_path = e.path().string();
        std::error_code ec_isdir;
        de.is_directory = e.is_directory(ec_isdir);
        result.push_back(std::move(de));
    }
    return result;
}

}  // namespace ase::utils::fs
