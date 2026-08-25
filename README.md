# ase-utils

**Design:** DSGN_016 (AEC — Engine-Kernsystem: Zeit, RNG, Strings)

[![Layer](https://img.shields.io/badge/Layer-0%20Foundation-blue.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()
[![Header Only](https://img.shields.io/badge/Header-Only-green.svg)]()

> Essential utilities for environment configuration and data encoding

Part of [ASE - Antares Simulation Engine](../../..)

## Overview

`ase-utils` provides foundational utilities for common tasks that don't warrant their own module: loading environment variables from `.env` files and Base64 encoding/decoding. The .env file loader parses KEY=VALUE pairs from a `.env` file at server startup, populating the process environment before any module reads its configuration — this is how MONGODB_URI, NEO4J_PASSWORD, and other secrets are injected in development without hardcoding. The Base64 encoder/decoder handles binary-to-text conversion for JWT token payloads in the auth system, binary data embedding in JSON messages, and asset data encoding for network transmission. Both utilities are header-only with zero ASE dependencies, following the Layer 0 foundation principle that any module can use them without introducing coupling. The .env loader supports comments (# prefix), empty lines, quoted values, and variable expansion — matching the dotenv convention used across the Node.js ecosystem for the ase-auth service compatibility. As the engine grows, additional foundational utilities (UUID generation, hash functions, string manipulation) may be added to this module rather than creating separate single-purpose libraries.

## Features

- **Dotenv**: Load environment variables from `.env` files with full parsing support
- **Base64**: RFC 4648 compliant encoding/decoding for binary data

## Installation

```cmake
# Add to your CMakeLists.txt
add_subdirectory(foundation/ase-utils)
target_link_libraries(your_target PRIVATE ase-utils)
```

Header-only library - include what you need:

```cpp
#include <ase/utils/dotenv.hpp>
#include <ase/utils/base64.hpp>
```

## Usage

### Dotenv - Environment Configuration

Load configuration from `.env` files at application startup.

**Dieses Modul LIEST die Datei nicht mehr selbst (2026-08-22).** `dotenv::load()` gab es bis
dahin, und es öffnete die Datei mit einem Eingabestrom — die einzigen zwei Verstöße, die
`ase-utils` je hatte. Die Regel nennt `ase::fileio` als Ziel, aber ein Foundation-Modul bindet
kein `ase::`-Ziel (siehe Abschnitt *Layer*). Also liest der **Rufer**, und dieses Modul parst
und setzt. Die Aufteilung kostet eine Zeile und hält die Schichtgrenze.

```cpp
#include <ase/utils/dotenv.hpp>
#include <ase/fileio/text_reader.hpp>   // beim RUFER, nicht in ase-utils

using namespace ase::utils;

int main() {
    // Der Rufer liest. file_exists VOR read_lines, weil eine leere Liste sonst
    // "Datei fehlt" und "Datei leer" nicht unterscheidet.
    if (!ase::fileio::file_exists(".env")) {
        std::cerr << "No .env file in working directory" << std::endl;
        return 1;
    }

    // Dieses Modul parst und setzt. Rückgabe: Anzahl gesetzter Variablen.
    const auto lines = ase::fileio::read_lines(".env");
    const int  count = dotenv::apply_lines(lines);

    // Bestehende Umgebung gewinnt (Vorgabe). Zum Überschreiben:
    // dotenv::apply_lines(lines, true);

    // Einzelne Zeile parsen, ohne etwas zu setzen
    auto [key, value] = dotenv::parse_line("HTTP_PORT=8090  # Kommentar");

    // Lesen mit Vorgabewert
    std::string port = dotenv::get("HTTP_PORT", "8090");

    // Nur Vorhandensein prüfen
    if (dotenv::has("NEO4J_URI")) {
        // ...
    }

    return count >= 0 ? 0 : 1;
}
```

Die einparametrige `dotenv::get("KEY")`, die einen optionalen Wert lieferte, ist ebenfalls
gelöscht (2026-08-20) — sie hatte baumweit null Rufer. Es bleibt `get(key, default_value)`.

### .env File Format

```bash
# Comments start with #
MONGODB_URI=mongodb+srv://user:pass@cluster.mongodb.net
MONGODB_DATABASE=ase_engine

# Quoted values (quotes are removed)
NEO4J_URI="neo4j+s://bolt.graph.ase.antarien.com"
NEO4J_USER='neo4j'

# Inline comments
HTTP_PORT=8090 # Default HTTP port

# Empty lines are ignored

# Multiline values (must be quoted)
PRIVATE_KEY="-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASC...
-----END PRIVATE KEY-----"
```

### Supported .env Features

| Feature | Support | Example |
|---------|---------|---------|
| Key-value pairs | Yes | `KEY=value` |
| Double quotes | Yes | `KEY="value with spaces"` |
| Single quotes | Yes | `KEY='value with spaces'` |
| Comments | Yes | `# This is a comment` |
| Inline comments | Yes | `KEY=value # comment` |
| Empty lines | Yes | (ignored) |
| No quotes | Yes | `KEY=value` |
| Whitespace trimming | Yes | `  KEY  =  value  ` |

### Base64 - Data Encoding

Encode and decode binary data for transmission or storage:

```cpp
#include <ase/utils/base64.hpp>

using namespace ase::utils;

// Encode binary data
std::vector<uint8_t> binary_data = {0x48, 0x65, 0x6c, 0x6c, 0x6f};  // "Hello"
std::string encoded = Base64::encode(binary_data);
// Result: "SGVsbG8="

// Decode back to binary
std::vector<uint8_t> decoded = Base64::decode(encoded);
// Result: {0x48, 0x65, 0x6c, 0x6c, 0x6f}

// Encode string (convert to bytes first)
std::string text = "Hello, World!";
std::vector<uint8_t> bytes(text.begin(), text.end());
std::string base64 = Base64::encode(bytes);

// Common use cases
std::string encode_api_key(const std::string& key) {
    std::vector<uint8_t> bytes(key.begin(), key.end());
    return Base64::encode(bytes);
}

std::string decode_api_key(const std::string& encoded) {
    auto bytes = Base64::decode(encoded);
    return std::string(bytes.begin(), bytes.end());
}
```

### Real-World Example - Application Startup

```cpp
#include <ase/utils/dotenv.hpp>
#include <iostream>

struct Config {
    std::string mongodb_uri;
    std::string mongodb_database;
    std::string neo4j_uri;
    std::string neo4j_user;
    std::string neo4j_password;
    uint16_t http_port;
    uint16_t signaling_port;
};

Config load_config() {
    using namespace ase::utils;

    // Load environment variables - der Rufer liest, dieses Modul parst und setzt
    if (ase::fileio::file_exists(".env")) {
        dotenv::apply_lines(ase::fileio::read_lines(".env"));
    } else {
        std::cerr << "Warning: No .env file found, using defaults" << std::endl;
    }

    Config config;

    // Required variables (throw if missing). get() liefert IMMER einen String, deshalb
    // entscheidet ein leerer Vorgabewert ueber "fehlt" - die optionale Ueberladung ist
    // am 2026-08-20 geloescht worden, sie hatte keinen Rufer.
    config.mongodb_uri = dotenv::get("MONGODB_URI", "");
    if (config.mongodb_uri.empty()) {
        throw std::runtime_error("MONGODB_URI is required");
    }

    // Optional with defaults
    config.mongodb_database = dotenv::get("MONGODB_DATABASE", "ase_engine");
    config.http_port = std::stoi(dotenv::get("HTTP_PORT", "8090"));
    config.signaling_port = std::stoi(dotenv::get("SIGNALING_PORT", "9004"));

    // Neo4j (optional)
    if (dotenv::has("NEO4J_URI")) {
        config.neo4j_uri = dotenv::get("NEO4J_URI", "");
        config.neo4j_user = dotenv::get("NEO4J_USER", "neo4j");
        config.neo4j_password = dotenv::get("NEO4J_PASSWORD", "");
    }

    return config;
}

int main() {
    try {
        Config config = load_config();
        std::cout << "Database: " << config.mongodb_database << std::endl;
        std::cout << "HTTP Port: " << config.http_port << std::endl;
        // Start application...
    } catch (const std::exception& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
```

### Real-World Example - WebRTC Signaling

```cpp
#include <ase/utils/base64.hpp>
#include <nlohmann/json.hpp>

using namespace ase::utils;

// Encode SDP offer for transmission
std::string encode_sdp_offer(const std::string& sdp) {
    std::vector<uint8_t> bytes(sdp.begin(), sdp.end());
    return Base64::encode(bytes);
}

// Decode SDP answer from remote peer
std::string decode_sdp_answer(const std::string& encoded) {
    auto bytes = Base64::decode(encoded);
    return std::string(bytes.begin(), bytes.end());
}

// Example usage in WebRTC signaling
void handle_webrtc_offer(const nlohmann::json& msg) {
    // Decode offer
    std::string encoded_offer = msg["offer"];
    std::string sdp_offer = decode_sdp_answer(encoded_offer);

    // Process offer, generate answer
    std::string sdp_answer = generate_answer(sdp_offer);

    // Encode answer for transmission
    nlohmann::json response;
    response["type"] = "answer";
    response["answer"] = encode_sdp_offer(sdp_answer);

    // Send response...
}
```

### Real-World Example - Secure Token Storage

```cpp
#include <ase/utils/base64.hpp>
#include <ase/utils/dotenv.hpp>
#include <random>

using namespace ase::utils;

// Generate secure session token
std::string generate_session_token() {
    // Generate random bytes
    std::vector<uint8_t> random_bytes(32);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (auto& byte : random_bytes) {
        byte = static_cast<uint8_t>(dis(gen));
    }

    // Encode as Base64 for storage/transmission
    return Base64::encode(random_bytes);
}

// Verify session token
bool verify_session_token(const std::string& token) {
    try {
        auto bytes = Base64::decode(token);
        return bytes.size() == 32;  // Expected size
    } catch (...) {
        return false;  // Invalid Base64
    }
}
```

## API Reference

### dotenv

| Function | Description |
|----------|-------------|
| `load(path = ".env", overwrite = false)` | Load environment variables from file |
| `get(key)` | Get variable as `std::optional<std::string>` |
| `get(key, default_value)` | Get variable with default fallback |
| `has(key)` | Check if variable exists |

**Return Values:**
- `load()` returns number of variables loaded, or `-1` on error
- `get(key)` returns `std::optional<std::string>` (empty if not set)
- `get(key, default)` returns `std::string` (default if not set)
- `has(key)` returns `bool`

### Base64

| Function | Description |
|----------|-------------|
| `encode(data)` | Encode `std::vector<uint8_t>` to Base64 string |
| `decode(encoded)` | Decode Base64 string to `std::vector<uint8_t>` |

**Compliance:**
- RFC 4648 Base64 encoding
- Standard alphabet: `A-Za-z0-9+/`
- Padding character: `=`

## Design Philosophy

### Header-Only
All implementations are in headers for easy integration without linking.

### Cross-Platform
Works on Linux, macOS, and Windows with platform-specific adaptations:
- Windows: Uses `_putenv_s()`
- Unix: Uses `setenv()`

### Minimal Dependencies
Only depends on C++ standard library - no external dependencies.

### Security Considerations

#### Dotenv
- Environment variables are loaded into process environment (visible to `getenv()`)
- Sensitive data in `.env` files should be protected with file permissions (0600)
- **Never commit `.env` files to version control** - use `.env.example` instead

#### Base64
- Base64 is **encoding**, not **encryption** - data is not secure
- Use for transport/storage format, not for security
- Always encrypt sensitive data before Base64 encoding

## Best Practices

### .env File Management

```bash
# Good: Use .env.example as template (commit this)
cp .env.example .env

# Good: Ignore actual .env in .gitignore
echo ".env" >> .gitignore

# Good: Set restrictive permissions
chmod 600 .env

# Bad: Commit .env to Git
git add .env  # DON'T DO THIS
```

### Application Configuration

```cpp
// Good: einmal beim Start lesen und setzen
int main() {
    dotenv::apply_lines(ase::fileio::read_lines(".env"));
    Config config = load_config();
    // Use config throughout application
}

// Bad: mehrfach
void some_function() {
    dotenv::apply_lines(ase::fileio::read_lines(".env"));  // liest die Datei erneut
}
```

Im Baum macht das genau eine Stelle: `kernel_env_ldr_sys.cpp` in `on_start`. Wer eine zweite
braucht, hat vermutlich ein Konfigurationsproblem und kein Leseproblem.

### Base64 Usage

```cpp
// Good: Use for text-safe encoding
std::string encoded = Base64::encode(binary_data);
send_over_json(encoded);

// Bad: Use for encryption (it's not!)
std::string "encrypted" = Base64::encode(password);  // NOT SECURE!

// Good: Encrypt THEN encode
std::vector<uint8_t> encrypted = encrypt_aes(data);
std::string safe_to_transmit = Base64::encode(encrypted);
```

## Dependencies

### External
- C++20 standard library (`<string>`, `<filesystem>`, `<fstream>`, `<optional>`, `<vector>`, `<array>`)

### Internal
- None (Layer 0 - Foundation)

## License

Proprietary - ASE Engine

---

**Layer 0 Foundation** | No ASE dependencies | Header-only | C++20
