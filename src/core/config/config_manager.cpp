#include "core/config/config_manager.h"
#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <toml.hpp>

namespace ai_backend::core::config {

ConfigManager& ConfigManager::GetInstance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
}

bool ConfigManager::LoadFromFile(const std::string& file_path) {
    config_file_path_ = file_path;
    return Reload();
}

bool ConfigManager::Reload() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 清除现有配置
        config_values_.clear();
        
        // 解析TOML文件
        if (!ParseTomlFile(config_file_path_)) {
            return false;
        }
        
        // 加载环境变量
        LoadFromEnvironment();
        
        is_loaded_ = true;
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to reload configuration: {}", e.what());
        return false;
    }
}

bool ConfigManager::ParseTomlFile(const std::string& file_path) {
    try {
        // 解析TOML文件
        auto data = toml::parse(file_path);
        
        // 递归加载配置项
        LoadNestedConfig("", data);
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse TOML configuration file {}: {}", file_path, e.what());
        return false;
    }
}

void ConfigManager::LoadNestedConfig(const std::string& prefix, const toml::value& value) {
    // 处理各种类型的TOML值
    if (value.is_table()) {
        // 递归处理表
        for (const auto& [key, val] : value.as_table()) {
            std::string new_prefix = prefix.empty() ? key : prefix + "." + key;
            LoadNestedConfig(new_prefix, val);
        }
    } else if (value.is_array()) {
        // 处理数组
        const auto& array = value.as_array();
        
        if (!array.empty()) {
            if (array[0].is_string()) {
                std::vector<std::string> string_list;
                for (const auto& item : array) {
                    string_list.push_back(item.as_string());
                }
                Set(prefix, string_list);
            } else if (array[0].is_integer()) {
                std::vector<int> int_list;
                for (const auto& item : array) {
                    int_list.push_back(static_cast<int>(item.as_integer()));
                }
                Set(prefix, int_list);
            } else if (array[0].is_floating()) {
                std::vector<double> double_list;
                for (const auto& item : array) {
                    double_list.push_back(item.as_floating());
                }
                Set(prefix, double_list);
            }
        }
    } else if (value.is_string()) {
        Set(prefix, value.as_string());
    } else if (value.is_integer()) {
        Set(prefix, static_cast<int>(value.as_integer()));
    } else if (value.is_floating()) {
        Set(prefix, value.as_floating());
    } else if (value.is_boolean()) {
        Set(prefix, value.as_boolean());
    }
}

void ConfigManager::LoadFromEnvironment() {
    // 支持Heroku的PORT环境变量
    const char* heroku_port = std::getenv("PORT");
    if (heroku_port) {
        try {
            int port_num = std::stoi(heroku_port);
            Set("server.port", port_num);
        } catch (const std::exception& e) {
            spdlog::error("Invalid Heroku PORT value: {}", e.what());
        }
    } else {
        // 回退到自定义环境变量
        const char* port = std::getenv("SERVER_PORT");
        if (port) {
            try {
                int port_num = std::stoi(port);
                Set("server.port", port_num);
            } catch (const std::exception& e) {
                spdlog::error("Invalid server port in environment variable: {}", e.what());
            }
        }
    }
    
    // 支持Heroku的DATABASE_URL环境变量
    const char* database_url = std::getenv("DATABASE_URL");
    if (database_url) {
        // Heroku格式转换为PostgreSQL连接字符串
        std::string db_url = std::string(database_url);
        if (db_url.substr(0, 8) == "postgres:") {
            db_url = "postgresql" + db_url.substr(8);
        }
        Set("database.connection_string", db_url);
    } else {
        // 回退到自定义环境变量
        const char* db_url = std::getenv("DB_CONNECTION_STRING");
        if (db_url) {
            Set("database.connection_string", std::string(db_url));
        }
    }
    
    // 加载AI模型API密钥
    const char* wenxin_api_key = std::getenv("WENXIN_API_KEY");
    if (wenxin_api_key) {
        Set("ai.wenxin.api_key", std::string(wenxin_api_key));
    }
    
    const char* wenxin_api_secret = std::getenv("WENXIN_API_SECRET");
    if (wenxin_api_secret) {
        Set("ai.wenxin.api_secret", std::string(wenxin_api_secret));
    }
    
    const char* xunfei_api_key = std::getenv("XUNFEI_API_KEY");
    if (xunfei_api_key) {
        Set("ai.xunfei.api_key", std::string(xunfei_api_key));
    }
    
    const char* xunfei_app_id = std::getenv("XUNFEI_APP_ID");
    if (xunfei_app_id) {
        Set("ai.xunfei.app_id", std::string(xunfei_app_id));
    }
    
    const char* xunfei_api_secret = std::getenv("XUNFEI_API_SECRET");
    if (xunfei_api_secret) {
        Set("ai.xunfei.api_secret", std::string(xunfei_api_secret));
    }
    
    const char* tongyi_api_key = std::getenv("TONGYI_API_KEY");
    if (tongyi_api_key) {
        Set("ai.tongyi.api_key", std::string(tongyi_api_key));
    }
    
    const char* deepseek_api_key = std::getenv("DEEPSEEK_API_KEY");
    if (deepseek_api_key) {
        Set("ai.deepseek.api_key", std::string(deepseek_api_key));
    }
    
    // JWT密钥
    const char* jwt_secret = std::getenv("JWT_SECRET");
    if (jwt_secret) {
        Set("auth.jwt_secret", std::string(jwt_secret));
    }
}

void ConfigManager::Set(const std::string& key, const ConfigValue& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查值是否已存在且不同
    bool value_changed = false;
    auto it = config_values_.find(key);
    
    if (it != config_values_.end()) {
        if (it->second != value) {
            value_changed = true;
            it->second = value;
        }
    } else {
        config_values_[key] = value;
        value_changed = true;
    }
    
    // 通知配置变更
    if (value_changed) {
        NotifyConfigChange(key, value);
    }
}

std::string ConfigManager::GetString(const std::string& key, const std::string& default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<std::string>(it->second)) {
        return std::get<std::string>(it->second);
    }
    
    return default_value;
}

int ConfigManager::GetInt(const std::string& key, int default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<int>(it->second)) {
        return std::get<int>(it->second);
    }
    
    return default_value;
}

double ConfigManager::GetDouble(const std::string& key, double default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<double>(it->second)) {
        return std::get<double>(it->second);
    }
    
    return default_value;
}

bool ConfigManager::GetBool(const std::string& key, bool default_value) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<bool>(it->second)) {
        return std::get<bool>(it->second);
    }
    
    return default_value;
}

std::vector<std::string> ConfigManager::GetStringList(
    const std::string& key, 
    const std::vector<std::string>& default_value) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<std::vector<std::string>>(it->second)) {
        return std::get<std::vector<std::string>>(it->second);
    }
    
    return default_value;
}

std::vector<int> ConfigManager::GetIntList(
    const std::string& key,
    const std::vector<int>& default_value) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<std::vector<int>>(it->second)) {
        return std::get<std::vector<int>>(it->second);
    }
    
    return default_value;
}

std::vector<double> ConfigManager::GetDoubleList(
    const std::string& key,
    const std::vector<double>& default_value) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = config_values_.find(key);
    if (it != config_values_.end() && std::holds_alternative<std::vector<double>>(it->second)) {
        return std::get<std::vector<double>>(it->second);
    }
    
    return default_value;
}

bool ConfigManager::HasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_values_.find(key) != config_values_.end();
}

void ConfigManager::Remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_values_.erase(key);
}

void ConfigManager::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    config_values_.clear();
}

std::vector<std::string> ConfigManager::GetAllKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> keys;
    keys.reserve(config_values_.size());
    
    for (const auto& [key, _] : config_values_) {
        keys.push_back(key);
    }
    
    return keys;
}

void ConfigManager::RegisterChangeCallback(const std::string& key, ChangeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    change_callbacks_[key].push_back(callback);
}

void ConfigManager::UnregisterChangeCallback(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    change_callbacks_.erase(key);
}

void ConfigManager::NotifyConfigChange(const std::string& key, const ConfigValue& new_value) {
    // 查找对应key的回调
    auto it = change_callbacks_.find(key);
    if (it != change_callbacks_.end()) {
        for (const auto& callback : it->second) {
            callback(key, new_value);
        }
    }
    
    // 查找通配符回调
    auto wildcard_it = change_callbacks_.find("*");
    if (wildcard_it != change_callbacks_.end()) {
        for (const auto& callback : wildcard_it->second) {
            callback(key, new_value);
        }
    }
}

} // namespace ai_backend::core::config