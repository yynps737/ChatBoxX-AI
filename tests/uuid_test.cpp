#include "core/utils/uuid.h"
#include <gtest/gtest.h>
#include <regex>

namespace ai_backend::tests {

using namespace core::utils;

TEST(UuidGeneratorTest, GenerateUuid) {
    // 测试生成UUID格式是否正确
    std::string uuid = UuidGenerator::GenerateUuid();
    std::regex uuid_regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", 
                          std::regex_constants::icase);
    EXPECT_TRUE(std::regex_match(uuid, uuid_regex));
    
    // 测试多次生成的UUID是否唯一
    std::string uuid2 = UuidGenerator::GenerateUuid();
    EXPECT_NE(uuid, uuid2);
}

TEST(UuidGeneratorTest, GenerateTimeBasedUuid) {
    // 测试生成基于时间的UUID格式是否正确
    std::string uuid = UuidGenerator::GenerateTimeBasedUuid();
    std::regex uuid_regex("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$", 
                          std::regex_constants::icase);
    EXPECT_TRUE(std::regex_match(uuid, uuid_regex));
    
    // 测试版本标识是否为1（时间基础）
    std::string version_char = uuid.substr(14, 1);
    EXPECT_EQ(version_char, "1");
    
    // 测试多次生成的UUID是否唯一
    std::string uuid2 = UuidGenerator::GenerateTimeBasedUuid();
    EXPECT_NE(uuid, uuid2);
}

TEST(UuidGeneratorTest, IsValid) {
    // 测试有效UUID
    EXPECT_TRUE(UuidGenerator::IsValid("123e4567-e89b-12d3-a456-426614174000"));
    
    // 测试无效UUID
    EXPECT_FALSE(UuidGenerator::IsValid("invalid-uuid"));
    EXPECT_FALSE(UuidGenerator::IsValid("123e4567-e89b-12d3-a456-42661417400")); // 少一位
    EXPECT_FALSE(UuidGenerator::IsValid("123e4567-e89b-12d3-a456-4266141740001")); // 多一位
    EXPECT_FALSE(UuidGenerator::IsValid("123e4567-e89b-12d3-a456_426614174000")); // 格式错误
}

TEST(UuidGeneratorTest, ParseAndFormat) {
    std::string uuid_str = "123e4567-e89b-12d3-a456-426614174000";
    
    // 测试从字符串解析
    auto bytes = UuidGenerator::Parse(uuid_str);
    EXPECT_EQ(bytes.size(), 16);
    
    // 测试从字节重新格式化为字符串
    std::string formatted = UuidGenerator::Format(bytes);
    EXPECT_EQ(uuid_str, formatted);
}

} // namespace ai_backend::tests