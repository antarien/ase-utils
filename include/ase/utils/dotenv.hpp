#pragma once

/**
 * @file        dotenv.hpp
 * @brief       Parse KEY=value lines into the process environment
 * @description Turns the lines of a .env file into environment variables. Handles quoted
 *              values, inline comments and blank lines. One consumer in the tree:
 *              kernel_env_ldr_sys.cpp reads the file and hands the lines here at startup.
 *
 *              THIS HEADER NO LONGER OPENS FILES (2026-08-22), and that is the whole change:
 *              it parses and applies, the caller reads. See the tombstone under load().
 *
 * @module      ase-utils
 * @layer       0 (Foundation)
 * @category    config/environment
 * @created     2025-12-15
 * @modified    2026-08-22
 * @version     2.0.0
 */

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>
// No <optional>: the one function that returned it was deleted on 2026-08-20 (see below).
// No <string_view>: the three detail:: parsers took a view type until 2026-08-20. The
// forbidden-type rule points at char* or entt::hashed_string; neither fits a parser that hands
// its argument straight to substr/find_first_not_of, and the SSOT it cites lists raw char* under
// its own anti-patterns. const std::string& is what the call sites already pass - every one of
// them is a std::string or a substr of one - so the views bought nothing and cost a rule.

namespace ase::utils {

/**
 * Dotenv - turn .env lines into environment variables
 *
 * Supports:
 * - KEY=value
 * - KEY="quoted value"
 * - KEY='single quoted'
 * - # comments
 * - Empty lines
 * - Inline comments: KEY=value # comment
 *
 * Usage:
 *   auto lines = ase::fileio::read_lines(".env");        // the CALLER reads
 *   int n = ase::utils::dotenv::apply_lines(lines);      // this header applies
 *   auto val = ase::utils::dotenv::get("KEY", "def");    // read with a fallback
 *   bool set = ase::utils::dotenv::has("KEY");           // presence only
 */
namespace dotenv {

namespace detail {

inline std::string trim(const std::string& str) {
    const auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

inline std::string unquote(const std::string& str) {
    if (str.size() >= 2) {
        if ((str.front() == '"' && str.back() == '"') ||
            (str.front() == '\'' && str.back() == '\'')) {
            return str.substr(1, str.size() - 2);
        }
    }
    return str;
}

}  // namespace detail

/**
 * Split one line into (key, value). Returns an empty key for blank lines, comment lines and
 * lines without a separator - the caller skips those.
 */
inline std::pair<std::string, std::string> parse_line(const std::string& line) {
    // Skip empty lines and comments
    auto trimmed = detail::trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return {"", ""};
    }

    // Find the = separator
    const auto eq_pos = trimmed.find('=');
    if (eq_pos == std::string::npos) {
        return {"", ""};
    }

    auto key = detail::trim(trimmed.substr(0, eq_pos));
    auto value_part = trimmed.substr(eq_pos + 1);

    // Handle inline comments (but not inside quotes)
    std::string value;
    bool in_quotes = false;
    char quote_char = 0;

    for (size_t i = 0; i < value_part.size(); ++i) {
        char c = value_part[i];

        if (!in_quotes && (c == '"' || c == '\'')) {
            in_quotes = true;
            quote_char = c;
            value += c;
        } else if (in_quotes && c == quote_char) {
            in_quotes = false;
            value += c;
        } else if (!in_quotes && c == '#') {
            break;  // Start of comment
        } else {
            value += c;
        }
    }

    value = detail::trim(value);
    value = detail::unquote(value);

    return {key, value};
}

/**
 * Apply already-read lines to the process environment.
 * @param lines     the file's lines, in order
 * @param overwrite if true, replace variables that are already set
 * @return number of variables set
 */
inline int apply_lines(const std::vector<std::string>& lines, bool overwrite = false) {
    int count = 0;

    for (const auto& line : lines) {
        auto [key, value] = parse_line(line);

        if (key.empty()) {
            continue;
        }

        // Existing environment wins unless the caller says otherwise
        if (!overwrite && std::getenv(key.c_str()) != nullptr) {
            continue;
        }

        #ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
        #else
            setenv(key.c_str(), value.c_str(), overwrite ? 1 : 0);
        #endif

        ++count;
    }

    return count;
}

// DELETED 2026-08-22: `inline int load(const char* path = ".env", bool overwrite = false)`.
//
// It opened the file itself with an input stream, which is what the forbidden-type rule
// reports here - twice, and they were the only two findings this module had. The rule names
// the destination: ase::fileio. FOLLOWING IT LITERALLY WOULD HAVE COST MORE THAN IT SAVED.
//
// MEASURED, before deciding: `foundation/ase-utils/CMakeLists.txt` contains NO
// target_link_libraries at all. Binding ase::fileio would have been this module's FIRST
// dependency ever, and the first Layer-0-to-Layer-0 ase:: edge in the tree - of eleven
// foundation modules, zero bind an ase:: target today and six say so in their own build file
// ("VERBOTEN: No dependencies allowed in Layer 0!").
//
// So the reading moved to the ONE caller instead: KernelEnvLdrSystem, in Layer 2,
// where ase::fileio is bound without any question. It calls fileio::read_lines and hands the
// result to apply_lines above. The rule is satisfied - the stream is gone and its replacement
// IS ase::fileio - and the layer boundary is untouched. Same move as the ring buffer's
// `bool pop(T&)` on the same day: dissolve the coupling rather than relocate it.
//
// NOTHING WAS LOST, name by name: quoted values, single quotes, inline comments, blank and
// comment lines, the existing-environment-wins default and the overwrite flag all live in
// apply_lines. The one branch that could NOT move is the missing-file case, which load()
// reported as -1: read_lines returns an empty vector for "not there" and for "empty file"
// alike. The caller restores the distinction with fileio::file_exists before reading, and
// that check is the reason it is mentioned here rather than left to be rediscovered.
//
// DELETED 2026-08-20 as well: `inline std::optional<std::string> get(const std::string&)`.
// It returned an optional, which the forbidden-type rule points at ase::types::Option - and
// following that pointer would have pulled a new dependency for one function. Measured before
// deciding: the overload had ZERO callers across the whole tree. Dead code is deleted, not
// ported (R15).

/**
 * Get an environment variable value with default
 * @param key Variable name
 * @param default_value Value to return if not set
 * @return Variable value or default
 */
inline std::string get(const std::string& key, const std::string& default_value) {
    if (const char* value = std::getenv(key.c_str())) {
        return value;
    }
    return default_value;
}

/**
 * Check if an environment variable is set
 */
inline bool has(const std::string& key) {
    return std::getenv(key.c_str()) != nullptr;
}

}  // namespace dotenv
}  // namespace ase::utils
