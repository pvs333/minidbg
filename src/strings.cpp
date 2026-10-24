#include "strings.hpp"

#include <cctype>
#include <charconv>
#include <cstdio>

namespace minidbg {

std::vector<std::string> split_whitespace(std::string_view text) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        std::size_t start = i;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i > start) {
            tokens.emplace_back(text.substr(start, i - start));
        }
    }
    return tokens;
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

std::optional<std::uint64_t> parse_uint(std::string_view text) {
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    }
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::int64_t> parse_int(std::string_view text) {
    bool negative = false;
    if (!text.empty() && (text[0] == '-' || text[0] == '+')) {
        negative = text[0] == '-';
        text.remove_prefix(1);
    }
    auto magnitude = parse_uint(text);
    if (!magnitude) {
        return std::nullopt;
    }
    constexpr std::uint64_t kMaxPositive = static_cast<std::uint64_t>(INT64_MAX);
    if (negative) {
        if (*magnitude > kMaxPositive + 1) {
            return std::nullopt;
        }
        // Negate in unsigned arithmetic to avoid overflow for INT64_MIN.
        return static_cast<std::int64_t>(~*magnitude + 1);
    }
    if (*magnitude > kMaxPositive) {
        return std::nullopt;
    }
    return static_cast<std::int64_t>(*magnitude);
}

std::optional<std::uint8_t> parse_hex_byte(std::string_view text) {
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
    }
    if (text.empty() || text.size() > 2) {
        return std::nullopt;
    }
    unsigned value = 0;
    auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value, 16);
    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return static_cast<std::uint8_t>(value);
}

std::string to_hex(std::uint64_t value, int digits) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%0*llx", digits,
                  static_cast<unsigned long long>(value));
    return buffer;
}

}  // namespace minidbg
