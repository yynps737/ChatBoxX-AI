#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ai_backend::core::config {

// 配置管理器类
class ConfigManager {
public:
    // 配置类型
    using ConfigValue = std::variant
        std::string,
        int,
        double,
        bool,
        std::vector<std::string>,
        std::vector<int>,
        std::vector<double>
    >;
    
    // 获取单例实例
    static ConfigManager& GetInstance();
    
    // 从文件加载配置
    bool LoadFromFile(const std::string& file_path);
    
    // 从环境变量加载配置
    void LoadFromEnvironment();
    
    // 设置配置值
    void Set(const std::string& key, const ConfigValue& value);
    
    // 获取字符串值
    std::string GetString(const std::string& key, const std::string& default_value = "") const;
    
    // 获取整数值
    int GetInt(const std::string& key, int default_value = 0) const;
    
    // 获取浮点值
    double GetDouble(const std::string& key, double default_value = 0.0) const;
    
    // 获取布尔值
    bool GetBool(const std::string& key, bool default_value = false) const;
    
    // 获取字符串列表
    std::vector<std::string> GetStringList(const std::string& key, 
                                         const std::vector<std::string>& default_value = {}) const;
    
    // 获取整数列表
    std::vector<int> GetIntList(const std::string& key,
                              const std::vector<int>& default_value = {}) const;
    
    // 获取浮点列表
    std::vector<double> GetDoubleList(const std::string& key,
                                    const std::vector<double>& default_value = {}) const;
    
    // 检查键是否存在
    bool HasKey(const std::string& key) const;
    
    // 移除配置项
    void Remove(const std::string& key);
    
    // 清空所有配置
    void Clear();
    
    // 获取所有配置键
    std::vector<std::string> GetAllKeys() const;
    
    // 重新加载配置
    bool Reload();
    
    // 注册配置变更回调
    using ChangeCallback = std::function<void(const std::string& key, const ConfigValue& new_value)>;
    void RegisterChangeCallback(const std::string& key, ChangeCallback callback);
    
    // 取消注册配置变更回调
    void UnregisterChangeCallback(const std::string& key);

private:
    ConfigManager();
    
    // 禁止拷贝和移动
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    ConfigManager(ConfigManager&&) = delete;
    ConfigManager& operator=(ConfigManager&&) = delete;
    
    // 解析配置文件
    bool ParseTomlFile(const std::string& file_path);
    
    // 加载嵌套配置项
    void LoadNestedConfig(const std::string& prefix, const toml::value& value);
    
    // 通知配置变更
    void NotifyConfigChange(const std::string& key, const ConfigValue& new_value);

private:
    std::string config_file_path_;
    std::unordered_map<std::string, ConfigValue> config_values_;
    std::unordered_map<std::string, std::vector<ChangeCallback>> change_callbacks_;
    mutable std::mutex mutex_;
    std::atomic<bool> is_loaded_{false};
};

} // namespace ai_backend::core::config