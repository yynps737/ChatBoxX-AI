#include <gtest/gtest.h>
#include "core/utils/string_utils.h"
#include "core/utils/uuid.h"

namespace ai_backend::test {

// 字符串工具测试
TEST(StringUtilsTest, SplitByChar) {
    auto parts = ai_backend::core::utils::StringUtils::Split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3);
    EXPECT_EQ(parts[0], "a");
    EXPECT_EQ(parts[1], "b");
    EXPECT_EQ(parts[2], "c");
}

TEST(StringUtilsTest, JoinStrings) {
    std::vector<std::string> parts = {"a", "b", "c"};
    std::string joined = ai_backend::core::utils::StringUtils::Join(parts, ",");
    EXPECT_EQ(joined, "a,b,c");
}

// UUID工具测试
TEST(UuidTest, GenerateUuid) {
    std::string uuid = ai_backend::core::utils::UuidGenerator::GenerateUuid();
    EXPECT_EQ(uuid.length(), 36); // UUID长度为36个字符
    EXPECT_TRUE(ai_backend::core::utils::UuidGenerator::IsValid(uuid));
}

} // namespace ai_backend::test
