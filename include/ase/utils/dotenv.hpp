#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <optional>

namespace ase::utils {

/**
 * Dotenv - Load environment variables from .env files
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
 *   ase::utils::dotenv::load();                    // Load .env from cwd
 *   ase::utils::dotenv::load("/path/to/.env");    // Load specific file
 *   auto val = ase::utils::dotenv::get("KEY");    // Get with optional
 */
namespace dotenv {

namespace detail {

inline std::string trim(std::string_view str) {
    const auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    const auto end = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(start, end - start + 1));
}

inline std::string unquote(std::string_view str) {
    if (str.size() >= 2) {
        if ((str.front() == '"' && str.back() == '"') ||
            (str.front() == '\'' && str.back() == '\'')) {
            return std::string(str.substr(1, str.size() - 2));
        }
    }
    return std::string(str);
}

inline std::pair<std::string, std::string> parse_line(std::string_view line) {
    // Skip empty lines and comments
    auto trimmed = trim(line);
    if (trimmed.empty() || trimmed[0] == '#') {
        return {"", ""};
    }

    // Find the = separator
    const auto eq_pos = trimmed.find('=');
    if (eq_pos == std::string_view::npos) {
        return {"", ""};
    }

    auto key = trim(trimmed.substr(0, eq_pos));
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

    value = trim(value);
    value = unquote(value);

    return {std::string(key), value};
}

}  // namespace detail

/**
 * Load environment variables from a .env file
 * @param path Path to .env file (default: ".env" in current directory)
 * @param overwrite If true, overwrite existing environment variables
 * @return Number of variables loaded, or -1 on error
 */
inline int load(const std::filesystem::path& path = ".env", bool overwrite = false) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return -1;
    }

    int count = 0;
    std::string line;

    while (std::getline(file, line)) {
        auto [key, value] = detail::parse_line(line);

        if (key.empty()) {
            continue;
        }

        // Check if already set
        if (!overwrite && std::getenv(key.c_str()) != nullptr) {
            continue;
        }

        // Set environment variable
        #ifdef _WIN32
            _putenv_s(key.c_str(), value.c_str());
        #else
            setenv(key.c_str(), value.c_str(), overwrite ? 1 : 0);
        #endif

        ++count;
    }

    return count;
}

/**
 * Get an environment variable value
 * @param key Variable name
 * @return Optional containing value if set
 */
inline std::optional<std::string> get(const std::string& key) {
    if (const char* value = std::getenv(key.c_str())) {
        return std::string(value);
    }
    return std::nullopt;
}

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
