#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>

#include <pqxx/pqxx>
#include "core/async/task.h"

namespace ai_backend::core::db {

// 数据库连接池类
class ConnectionPool {
public:
    // 单例访问
    static ConnectionPool& GetInstance();
    
    // 初始化连接池
    bool Initialize(const std::string& connection_string, 
                  size_t min_connections = 5, 
                  size_t max_connections = 20);
    
    // 析构函数
    ~ConnectionPool();
    
    // 获取连接（同步版本）
    std::shared_ptr<pqxx::connection> GetConnection();
    
    // 获取连接（异步版本）
    async::Task<std::shared_ptr<pqxx::connection>> GetConnectionAsync();
    
    // 释放连接
    void ReleaseConnection(std::shared_ptr<pqxx::connection> connection);
    
    // 关闭所有连接
    void CloseAll();
    
    // 获取连接池状态
    struct PoolStats {
        size_t active_connections;
        size_t idle_connections;
        size_t pending_requests;
    };
    PoolStats GetStats() const;

private:
    // 私有构造函数
    ConnectionPool();
    
    // 禁止拷贝和移动
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    ConnectionPool(ConnectionPool&&) = delete;
    ConnectionPool& operator=(ConnectionPool&&) = delete;
    
    // 创建新连接
    std::shared_ptr<pqxx::connection> CreateConnection();
    
    // 初始化连接池
    void InitializePool();
    
    // 监控空闲连接
    void MonitorConnections();
    
    // 测试连接是否有效
    bool TestConnection(pqxx::connection& conn);

private:
    std::string connection_string_;
    size_t min_connections_;
    size_t max_connections_;
    
    // 连接池
    std::deque<std::shared_ptr<pqxx::connection>> idle_connections_;
    std::unordered_set<std::shared_ptr<pqxx::connection>> active_connections_;
    
    // 连接管理
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::atomic<bool> shutdown_;
    
    // 连接监控
    std::thread monitor_thread_;
    std::chrono::seconds idle_timeout_{300}; // 默认5分钟
    
    // 统计信息
    std::atomic<size_t> pending_requests_{0};
};

} // namespace ai_backend::core::db