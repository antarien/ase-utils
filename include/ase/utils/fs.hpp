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
 * @created     2026-04-13
 * @modified    2026-04-13
 * @version     1.0.0
 */

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ase::utils::fs {

/**
 * Path - thin wrapper around std::string with path-like operations.
 * Stores a normalized POSIX-style string. Implicit conversion to std::string_view
 * lets Path be passed directly to all fs:: query functions.
 */
class Path {
public:
    Path() = default;
    explicit Path(const char* p) : m_path(p ? p : "") {}
    explicit Path(std::string p) : m_path(std::move(p)) {}
    explicit Path(std::string_view p) : m_path(p) {}

    Path operator/(std::string_view seg) const {
        if (m_path.empty()) return Path(std::string(seg));
        if (m_path.back() == '/') return Path(m_path + std::string(seg));
        return Path(m_path + "/" + std::string(seg));
    }

    const std::string& str() const noexcept { return m_path; }
    const char* c_str() const noexcept { return m_path.c_str(); }
    bool empty() const noexcept { return m_path.empty(); }

    operator std::string_view() const noexcept { return m_path; }

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
inline bool exists(std::string_view path) {
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(std::string(path)), ec);
}

/** True if path exists and is a directory. */
inline bool is_directory(std::string_view path) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(std::string(path)), ec);
}

/** True if path exists and is a regular file. */
inline bool is_regular_file(std::string_view path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(std::filesystem::path(std::string(path)), ec);
}

/** Return the parent directory of path. "/a/b/c" -> "/a/b". */
inline std::string parent_of(std::string_view path) {
    std::filesystem::path p((std::string(path)));
    return p.parent_path().string();
}

/** Return the filename component of path. "/a/b/c.txt" -> "c.txt". */
inline std::string filename_of(std::string_view path) {
    std::filesystem::path p((std::string(path)));
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
inline bool create_directories(std::string_view path) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(std::string(path)), ec);
    return !ec;
}

/**
 * Remove the filesystem entry at path. Works for both regular files and
 * directories; directory removal is recursive. Returns true on success (or
 * when the entry did not exist), false on error (permission denied,
 * cross-device link, etc.).
 */
inline bool remove(std::string_view path) {
    std::error_code ec;
    std::filesystem::path p((std::string(path)));
    if (std::filesystem::is_directory(p, ec)) {
        std::filesystem::remove_all(p, ec);
    } else {
        std::filesystem::remove(p, ec);
    }
    return !ec;
}

/** Return path relative to base. relative_to("/a/b/c", "/a") -> "b/c". */
inline std::string relative_to(std::string_view path, std::string_view base) {
    std::error_code ec;
    auto rel = std::filesystem::relative(
        std::filesystem::path(std::string(path)),
        std::filesystem::path(std::string(base)),
        ec);
    if (ec) return std::string(path);
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
inline std::vector<DirEntry> list_directory(std::string_view dir) {
    std::vector<DirEntry> result;
    std::filesystem::path dir_path((std::string(dir)));
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
