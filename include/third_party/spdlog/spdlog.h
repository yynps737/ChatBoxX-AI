// 简化的spdlog头文件，仅供构建示例使用
#ifndef SPDLOG_H
#define SPDLOG_H

#include <string>
#include <iostream>

namespace spdlog {
    enum class level { trace, debug, info, warn, err, critical, off };
    
    inline void trace(const std::string& msg) { std::cout << "[trace] " << msg << std::endl; }
    inline void debug(const std::string& msg) { std::cout << "[debug] " << msg << std::endl; }
    inline void info(const std::string& msg) { std::cout << "[info] " << msg << std::endl; }
    inline void warn(const std::string& msg) { std::cout << "[warn] " << msg << std::endl; }
    inline void error(const std::string& msg) { std::cout << "[error] " << msg << std::endl; }
    inline void critical(const std::string& msg) { std::cout << "[critical] " << msg << std::endl; }
    
    template<typename... Args>
    inline void trace(const std::string& fmt, const Args&... args) { 
        std::cout << "[trace] " << fmt << std::endl; 
    }
    
    template<typename... Args>
    inline void debug(const std::string& fmt, const Args&... args) { 
        std::cout << "[debug] " << fmt << std::endl; 
    }
    
    template<typename... Args>
    inline void info(const std::string& fmt, const Args&... args) { 
        std::cout << "[info] " << fmt << std::endl; 
    }
    
    template<typename... Args>
    inline void warn(const std::string& fmt, const Args&... args) { 
        std::cout << "[warn] " << fmt << std::endl; 
    }
    
    template<typename... Args>
    inline void error(const std::string& fmt, const Args&... args) { 
        std::cout << "[error] " << fmt << std::endl; 
    }
    
    template<typename... Args>
    inline void critical(const std::string& fmt, const Args&... args) { 
        std::cout << "[critical] " << fmt << std::endl; 
    }
    
    inline void set_level(level l) {}
    inline void set_pattern(const std::string& pattern) {}
}

#endif // SPDLOG_H
