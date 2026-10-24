#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace minidbg {

// Splits on runs of whitespace; never returns empty tokens.
std::vector<std::string> split_whitespace(std::string_view text);

std::string_view trim(std::string_view text);

// Parses an unsigned integer. Accepts decimal ("42") and hexadecimal ("0x2a"). Returns
// nullopt on any malformed input or overflow instead of silently truncating.
std::optional<std::uint64_t> parse_uint(std::string_view text);

// Like parse_uint but accepts an optional leading '-' or '+'.
std::optional<std::int64_t> parse_int(std::string_view text);

// Parses a hexadecimal byte with or without "0x" ("cc", "0xCC").
std::optional<std::uint8_t> parse_hex_byte(std::string_view text);

// "0x" followed by exactly `digits` zero-padded hex digits (or as many as needed if 0).
std::string to_hex(std::uint64_t value, int digits = 0);

}  // namespace minidbg
