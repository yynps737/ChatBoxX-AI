#include "core/utils/string_utils.h"
#include <gtest/gtest.h>

namespace ai_backend::tests {

using namespace core::utils;

TEST(StringUtilsTest, Split) {
    std::string str = "a,b,c,d";
    auto parts = StringUtils::Split(str, ',');
    ASSERT_EQ(parts.size(), 4);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
    EXPECT_EQ(parts[3], "d");
}

TEST(StringUtilsTest, SplitWithDelimiter) {
    std::string str = "a::b::c::d";
    auto parts = StringUtils::Split(str, "::");
    ASSERT_EQ(parts.size(), 4);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
    EXPECT_EQ(parts[3], "d");
}

TEST(StringUtilsTest, Join) {
    std::vector<std::string> parts = {"a", "b", "c", "d"};
    auto joined = StringUtils::Join(parts, ",");
    EXPECT_EQ(joined, "a,b,c,d");
}

TEST(StringUtilsTest, ToLower) {
    EXPECT_EQ(StringUtils::ToLower("Hello World"), "hello world");
}

TEST(StringUtilsTest, ToUpper) {
    EXPECT_EQ(StringUtils::ToUpper("Hello World"), "HELLO WORLD");
}

TEST(StringUtilsTest, Trim) {
    EXPECT_EQ(StringUtils::Trim("  Hello World  "), "Hello World");
}

TEST(StringUtilsTest, TrimLeft) {
    EXPECT_EQ(StringUtils::TrimLeft("  Hello World  "), "Hello World  ");
}

TEST(StringUtilsTest, TrimRight) {
    EXPECT_EQ(StringUtils::TrimRight("  Hello World  "), "  Hello World");
}

TEST(StringUtilsTest, Replace) {
    EXPECT_EQ(StringUtils::Replace("Hello World", "World", "C++"), "Hello C++");
}

TEST(StringUtilsTest, StartsWith) {
    EXPECT_TRUE(StringUtils::StartsWith("Hello World", "Hello"));
    EXPECT_FALSE(StringUtils::StartsWith("Hello World", "World"));
}

TEST(StringUtilsTest, EndsWith) {
    EXPECT_TRUE(StringUtils::EndsWith("Hello World", "World"));
    EXPECT_FALSE(StringUtils::EndsWith("Hello World", "Hello"));
}

TEST(StringUtilsTest, UrlEncode) {
    EXPECT_EQ(StringUtils::UrlEncode("Hello World"), "Hello+World");
    EXPECT_EQ(StringUtils::UrlEncode("a=1&b=2"), "a%3D1%26b%3D2");
}

TEST(StringUtilsTest, UrlDecode) {
    EXPECT_EQ(StringUtils::UrlDecode("Hello+World"), "Hello World");
    EXPECT_EQ(StringUtils::UrlDecode("a%3D1%26b%3D2"), "a=1&b=2");
}

TEST(StringUtilsTest, HtmlEscape) {
    EXPECT_EQ(StringUtils::HtmlEscape("<script>alert('XSS');</script>"), 
              "&lt;script&gt;alert(&#39;XSS&#39;);&lt;/script&gt;");
}

TEST(StringUtilsTest, GenerateRandomString) {
    auto str1 = StringUtils::GenerateRandomString(10);
    auto str2 = StringUtils::GenerateRandomString(10);
    EXPECT_EQ(str1.length(), 10);
    EXPECT_EQ(str2.length(), 10);
    EXPECT_NE(str1, str2);
}

TEST(StringUtilsTest, FormatBytes) {
    EXPECT_EQ(StringUtils::FormatBytes(512), "512 B");
    EXPECT_EQ(StringUtils::FormatBytes(1024), "1.00 KB");
    EXPECT_EQ(StringUtils::FormatBytes(1024 * 1024), "1.00 MB");
    EXPECT_EQ(StringUtils::FormatBytes(1024 * 1024 * 1024), "1.00 GB");
}

TEST(StringUtilsTest, FormatDuration) {
    EXPECT_EQ(StringUtils::FormatDuration(std::chrono::milliseconds(1000)), "1s");
    EXPECT_EQ(StringUtils::FormatDuration(std::chrono::milliseconds(60 * 1000)), "1m 0s");
    EXPECT_EQ(StringUtils::FormatDuration(std::chrono::milliseconds(60 * 60 * 1000)), "1h 0m 0s");
    EXPECT_EQ(StringUtils::FormatDuration(std::chrono::milliseconds(24 * 60 * 60 * 1000)), "1d 0h 0m 0s");
}

} // namespace ai_backend::tests