#include <gtest/gtest.h>

#include "strings.hpp"

using namespace minidbg;

TEST(Strings, SplitWhitespaceDropsEmptyTokens) {
    auto tokens = split_whitespace("  break   main\t 3 ");
    ASSERT_EQ(tokens.size(), 3u);
    EXPECT_EQ(tokens[0], "break");
    EXPECT_EQ(tokens[1], "main");
    EXPECT_EQ(tokens[2], "3");
    EXPECT_TRUE(split_whitespace("   ").empty());
}

TEST(Strings, Trim) {
    EXPECT_EQ(trim("  a b \n"), "a b");
    EXPECT_EQ(trim(""), "");
}

TEST(Strings, ParseUintAcceptsDecimalAndHex) {
    EXPECT_EQ(parse_uint("42"), 42u);
    EXPECT_EQ(parse_uint("0x2a"), 42u);
    EXPECT_EQ(parse_uint("0X2A"), 42u);
    EXPECT_EQ(parse_uint("0xffffffffffffffff"), UINT64_MAX);
}

TEST(Strings, ParseUintRejectsGarbageAndOverflow) {
    EXPECT_FALSE(parse_uint(""));
    EXPECT_FALSE(parse_uint("0x"));
    EXPECT_FALSE(parse_uint("12abc"));
    EXPECT_FALSE(parse_uint("-1"));
    EXPECT_FALSE(parse_uint("0x10000000000000000"));
}

TEST(Strings, ParseIntHandlesSignsAndLimits) {
    EXPECT_EQ(parse_int("-5"), -5);
    EXPECT_EQ(parse_int("+0x10"), 16);
    EXPECT_EQ(parse_int("-9223372036854775808"), INT64_MIN);
    EXPECT_FALSE(parse_int("9223372036854775808"));
}

TEST(Strings, ParseHexByte) {
    EXPECT_EQ(parse_hex_byte("cc"), 0xcc);
    EXPECT_EQ(parse_hex_byte("0x0A"), 0x0a);
    EXPECT_FALSE(parse_hex_byte("100"));
    EXPECT_FALSE(parse_hex_byte("zz"));
}

TEST(Strings, ToHex) {
    EXPECT_EQ(to_hex(255), "0xff");
    EXPECT_EQ(to_hex(255, 4), "0x00ff");
}
