#pragma once

#include <array>
#include <string>

namespace ai_backend::core::utils {

class UuidGenerator {
public:
    static std::string GenerateUuid();
    static std::string GenerateTimeBasedUuid();
    static std::string GenerateNameBasedUuid(const std::string& name, const std::string& namespace_uuid);
    
    static bool IsValid(const std::string& uuid);
    
    static std::array<uint8_t, 16> Parse(const std::string& uuid);
    static std::string Format(const std::array<uint8_t, 16>& bytes);
};

} // namespace ai_backend::core::utils