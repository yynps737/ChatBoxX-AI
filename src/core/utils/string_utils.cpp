#include "core/utils/string_utils.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <iomanip>
#include <random>
#include <cctype>

namespace ai_backend::core::utils {

std::vector<std::string> StringUtils::Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    
    return tokens;
}

std::vector<std::string> StringUtils::Split(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);
    
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }
    
    tokens.push_back(str.substr(start));
    return tokens;
}

std::string StringUtils::Join(const std::vector<std::string>& parts, const std::string& delimiter) {
    std::ostringstream result;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            result << delimiter;
        }
        result << parts[i];
    }
    return result.str();
}

std::string StringUtils::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

std::string StringUtils::ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

std::string StringUtils::Trim(const std::string& str) {
    return TrimRight(TrimLeft(str));
}

std::string StringUtils::TrimLeft(const std::string& str) {
    std::string result = str;
    result.erase(result.begin(), std::find_if(result.begin(), result.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return result;
}

std::string StringUtils::TrimRight(const std::string& str) {
    std::string result = str;
    result.erase(std::find_if(result.rbegin(), result.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), result.end());
    return result;
}

std::string StringUtils::Replace(const std::string& str, const std::string& from, const std::string& to) {
    std::string result = str;
    size_t start_pos = 0;
    while ((start_pos = result.find(from, start_pos)) != std::string::npos) {
        result.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }
    return result;
}

bool StringUtils::StartsWith(const std::string& str, const std::string& prefix) {
    return str.size() >= prefix.size() && str.compare(0, prefix.size(), prefix) == 0;
}

bool StringUtils::EndsWith(const std::string& str, const std::string& suffix) {
    return str.size() >= suffix.size() && 
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string StringUtils::UrlEncode(const std::string& str) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : str) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << std::setw(2) << int(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

std::string StringUtils::UrlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%') {
            if (i + 2 < str.size()) {
                int value = 0;
                std::istringstream hex_stream(str.substr(i + 1, 2));
                if (hex_stream >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += '%';
                }
            } else {
                result += '%';
            }
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }

    return result;
}

std::string StringUtils::HtmlEscape(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string StringUtils::GenerateRandomString(size_t length) {
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(generator)];
    }
    
    return result;
}

std::string StringUtils::FormatBytes(uint64_t bytes) {
    const char* suffix[] = {"B", "KB", "MB", "GB", "TB"};
    const int suffix_count = 5;
    
    int i = 0;
    double dblBytes = static_cast<double>(bytes);
    
    if (bytes > 1024) {
        for (i = 0; (bytes / 1024) > 0 && i < suffix_count - 1; i++, bytes /= 1024) {
            dblBytes = bytes / 1024.0;
        }
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << dblBytes << ' ' << suffix[i];
    
    return oss.str();
}

std::string StringUtils::FormatDuration(std::chrono::milliseconds duration) {
    auto ms = duration.count();
    
    uint64_t seconds = ms / 1000;
    ms %= 1000;
    
    uint64_t minutes = seconds / 60;
    seconds %= 60;
    
    uint64_t hours = minutes / 60;
    minutes %= 60;
    
    std::ostringstream oss;
    
    if (hours > 0) {
        oss << hours << "h ";
    }
    if (minutes > 0 || hours > 0) {
        oss << minutes << "m ";
    }
    oss << seconds << "." << std::setw(3) << std::setfill('0') << ms << "s";
    
    return oss.str();
}

std::string StringUtils::Base64Encode(const unsigned char* data, size_t length) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string ret;
    ret.reserve(((length + 2) / 3) * 4);
    
    for (size_t i = 0; i < length; i += 3) {
        size_t chunk = 0;
        chunk |= static_cast<size_t>(data[i]) << 16;
        
        if (i + 1 < length) {
            chunk |= static_cast<size_t>(data[i + 1]) << 8;
        }
        
        if (i + 2 < length) {
            chunk |= static_cast<size_t>(data[i + 2]);
        }
        
        for (int j = 0; j < 4; ++j) {
            if (i + j <= length) {
                ret.push_back(base64_chars[(chunk >> (18 - 6 * j)) & 0x3F]);
            } else {
                ret.push_back('=');
            }
        }
    }
    
    return ret;
}

std::vector<unsigned char> StringUtils::Base64Decode(const std::string& encoded) {
    static const std::string base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::vector<unsigned char> ret;
    size_t padding = 0;
    
    for (auto c : encoded) {
        if (c == '=') {
            padding++;
        }
    }
    
    ret.reserve(((encoded.size() + 3) / 4) * 3 - padding);
    
    for (size_t i = 0; i < encoded.size(); i += 4) {
        size_t chunk = 0;
        
        for (int j = 0; j < 4; ++j) {
            if (i + j < encoded.size() && encoded[i + j] != '=') {
                chunk |= base64_chars.find(encoded[i + j]) << (18 - 6 * j);
            }
        }
        
        for (int j = 0; j < 3; ++j) {
            if (i + j + 1 < encoded.size() && encoded[i + j + 1] != '=') {
                ret.push_back((chunk >> (16 - 8 * j)) & 0xFF);
            }
        }
    }
    
    return ret;
}

} // namespace ai_backend::core::utils
