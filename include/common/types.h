#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <cstdint>

namespace ai_backend::common::types {

using UUID = std::string;
using Timestamp = std::chrono::system_clock::time_point;
using TimestampString = std::string;
using Bytes = std::vector<uint8_t>;
using MemorySize = size_t;
using Duration = std::chrono::milliseconds;

struct Range {
    int64_t start;
    int64_t end;
};

struct PageInfo {
    int page;
    int page_size;
    int total_count;
    int total_pages;
};

} // namespace ai_backend::common::types