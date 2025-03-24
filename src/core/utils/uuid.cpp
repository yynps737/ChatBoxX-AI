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

// 验证UUID格式
bool UuidGenerator::IsValid(const std::string& uuid) {
    static const std::regex uuid_regex(
        "^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$",
        std::regex_constants::icase
    );
    
    return std::regex_match(uuid, uuid_regex);
}

// 生成时间戳基础的UUID (版本1)
std::string UuidGenerator::GenerateTimeBasedUuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> nodeDist(0, UINT16_MAX);
    
    // 获取当前时间戳
    using namespace std::chrono;
    auto now = system_clock::now();
    auto epochTime = now.time_since_epoch();
    auto timestamp = duration_cast<nanoseconds>(epochTime).count();
    
    // UUID时间从1582-10-15开始，需要加上这个差值
    constexpr int64_t uuid_epoch_diff = 122192928000000000; // 100ns ticks between UUID epoch and Unix epoch
    int64_t uuid_time = timestamp / 100 + uuid_epoch_diff;
    
    // 时间低位，中位，高位
    uint32_t time_low = static_cast<uint32_t>(uuid_time & 0xFFFFFFFF);
    uint16_t time_mid = static_cast<uint16_t>((uuid_time >> 32) & 0xFFFF);
    uint16_t time_hi_version = static_cast<uint16_t>((uuid_time >> 48) & 0x0FFF);
    time_hi_version |= (1 << 12); // 版本1
    
    // 时钟序列
    uint16_t clock_seq = (nodeDist(gen) & 0x3FFF) | 0x8000; // 设置变体
    
    // 节点ID (通常是MAC地址，这里用随机数模拟)
    uint16_t node[3];
    for (int i = 0; i < 3; ++i) {
        node[i] = nodeDist(gen);
    }
    
    // 格式化为UUID字符串
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    oss << std::setw(8) << time_low << "-";
    oss << std::setw(4) << time_mid << "-";
    oss << std::setw(4) << time_hi_version << "-";
    oss << std::setw(4) << clock_seq << "-";
    oss << std::setw(4) << node[0] << std::setw(4) << node[1] << std::setw(4) << node[2];
    
    return oss.str();
}

// 生成命名空间UUID (版本5，基于SHA1)
std::string UuidGenerator::GenerateNameBasedUuid(const std::string& name, const std::string& namespace_uuid) {
    // 注意：这是一个简化实现，实际上需要使用SHA1哈希算法
    // 在实际项目中应使用OpenSSL的SHA1实现
    
    // 在这个简化实现中，我们使用随机值而不是实际的哈希
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
    
    // 设置版本5和变体位
    data[1] = (data[1] & 0xFFFF0FFF) | 0x5000;  // 版本5
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

// 解析UUID为二进制形式
std::array<uint8_t, 16> UuidGenerator::Parse(const std::string& uuid) {
    std::array<uint8_t, 16> bytes{};
    
    if (!IsValid(uuid)) {
        return bytes;
    }
    
    // 移除破折号
    std::string hex_str;
    for (char c : uuid) {
        if (c != '-') {
            hex_str += c;
        }
    }
    
    // 解析十六进制字符串
    for (size_t i = 0; i < 16; ++i) {
        std::string byte_str = hex_str.substr(i * 2, 2);
        bytes[i] = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
    }
    
    return bytes;
}

// 从二进制形式生成UUID字符串
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