#include "core/utils/uuid.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <regex>

namespace ai_backend::core::utils {

std::string UuidGenerator::GenerateUuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    
    // 生成16个随机字节
    uint32_t data[4] = {
        dist(gen),
        dist(gen),
        dist(gen),
        dist(gen)
    };
    
    // 设置版本4和变体位
    data[1] = (data[1] & 0xFFFF0FFF) | 0x4000;  // 版本4
    data[2] = (data[2] & 0x3FFFFFFF) | 0x80000000;  // 变体
    
    // 格式化为UUID字符串
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    oss << std::setw(8) << data[0] << "-";
    oss << std::setw(4) << (data[1] >> 16) << "-";
    oss << std::setw(4) << (data[1] & 0xFFFF) << "-";
    oss << std::setw(4) << (data[2] >> 16) << "-";
    oss << std::setw(4) << (data[2] & 0xFFFF);
    oss << std::setw(8) << data[3];
    
    return oss.str();
}

bool UuidGenerator::IsValid(const std::string& uuid) {
    static const std::regex uuid_regex(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
        std::regex_constants::icase
    );
    
    return std::regex_match(uuid, uuid_regex);
}

} // namespace ai_backend::core::utils
