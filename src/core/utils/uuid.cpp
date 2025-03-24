#include "core/utils/uuid.h"
#include <random>
#include <sstream>
#include <iomanip>
#include <regex>

namespace ai_backend::core::utils {

// 生成随机UUID (版本4)
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

// 生成基于时间的UUID (版本1)
std::string UuidGenerator::GenerateTimeBasedUuid() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    
    // 生成16个随机字节
    uint32_t node_data = dist(gen);
    
    // 格式化为UUID字符串
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    // 时间低位
    oss << std::setw(8) << (timestamp & 0xFFFFFFFF) << "-";
    // 时间中位
    oss << std::setw(4) << ((timestamp >> 32) & 0xFFFF) << "-";
    // 时间高位和版本
    oss << std::setw(4) << (((timestamp >> 48) & 0x0FFF) | 0x1000) << "-";
    // 变体和序列
    oss << std::setw(4) << ((dist(gen) & 0x3FFF) | 0x8000) << "-";
    // 节点ID
    oss << std::setw(12) << node_data;
    
    return oss.str();
}

// 生成基于名字和命名空间的UUID (版本5)
std::string UuidGenerator::GenerateNameBasedUuid(const std::string& name, const std::string& namespace_uuid) {
    // 简化实现，实际应该使用SHA-1哈希算法
    std::hash<std::string> hasher;
    size_t hash_value = hasher(namespace_uuid + name);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    
    // 格式化为UUID字符串
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    oss << std::setw(8) << (hash_value & 0xFFFFFFFF) << "-";
    oss << std::setw(4) << ((hash_value >> 32) & 0xFFFF) << "-";
    // 版本5
    oss << std::setw(4) << (((hash_value >> 48) & 0x0FFF) | 0x5000) << "-";
    // 变体和序列
    oss << std::setw(4) << ((dist(gen) & 0x3FFF) | 0x8000) << "-";
    oss << std::setw(12) << dist(gen);
    
    return oss.str();
}

// 验证UUID格式
bool UuidGenerator::IsValid(const std::string& uuid) {
    static const std::regex uuid_regex(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
        std::regex_constants::icase
    );
    
    return std::regex_match(uuid, uuid_regex);
}

// 解析UUID字符串为字节数组
std::array<uint8_t, 16> UuidGenerator::Parse(const std::string& uuid) {
    if (!IsValid(uuid)) {
        throw std::invalid_argument("Invalid UUID format");
    }
    
    std::array<uint8_t, 16> bytes;
    
    // 移除连字符
    std::string hex_str = uuid;
    hex_str.erase(std::remove(hex_str.begin(), hex_str.end(), '-'), hex_str.end());
    
    // 解析十六进制字符串
    for (size_t i = 0; i < 16; ++i) {
        std::string byte_str = hex_str.substr(i * 2, 2);
        bytes[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return bytes;
}

// 格式化字节数组为UUID字符串
std::string UuidGenerator::Format(const std::array<uint8_t, 16>& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < 16; ++i) {
        oss << std::setw(2) << static_cast<int>(bytes[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            oss << "-";
        }
    }
    
    return oss.str();
}

} // namespace ai_backend::core::utils
