#include "core/db/connection_pool.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::db {

using namespace async;

ConnectionPool& ConnectionPool::GetInstance() {
    static ConnectionPool instance;
    return instance;
}

ConnectionPool::ConnectionPool() : shutdown_(false) {
}

ConnectionPool::~ConnectionPool() {
    CloseAll();
}

bool ConnectionPool::Initialize(const std::string& connection_string, 
                            size_t min_connections, 
                            size_t max_connections) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!idle_connections_.empty() || !active_connections_.empty()) {
        spdlog::warn("Connection pool already initialized");
        return false;
    }
    
    connection_string_ = connection_string;
    min_connections_ = min_connections;
    max_connections_ = max_connections;
    
    // 初始化连接池
    try {
        InitializePool();
        
        // 启动监控线程
        shutdown_ = false;
        monitor_thread_ = std::thread(&ConnectionPool::MonitorConnections, this);
        
        spdlog::info("Database connection pool initialized with {} connections", min_connections_);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to initialize connection pool: {}", e.what());
        return false;
    }
}

void ConnectionPool::InitializePool() {
    // 创建初始连接
    for (size_t i = 0; i < min_connections_; ++i) {
        try {
            auto conn = CreateConnection();
            idle_connections_.push_back(conn);
        } catch (const std::exception& e) {
            spdlog::error("Failed to create initial connection: {}", e.what());
            throw;
        }
    }
}

std::shared_ptr<pqxx::connection> ConnectionPool::GetConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    pending_requests_++;
    
    // 等待可用连接
    condition_.wait(lock, [this]() {
        return !idle_connections_.empty() || active_connections_.size() < max_connections_ || shutdown_;
    });
    
    pending_requests_--;
    
    if (shutdown_) {
        throw std::runtime_error("Connection pool is shutting down");
    }
    
    std::shared_ptr<pqxx::connection> conn;
    
    if (!idle_connections_.empty()) {
        // 有空闲连接可用
        conn = idle_connections_.front();
        idle_connections_.pop_front();
    } else if (active_connections_.size() < max_connections_) {
        // 创建新连接
        conn = CreateConnection();
    } else {
        // 不应该到达这里，因为条件变量已经确保了有连接可用
        throw std::runtime_error("No connections available");
    }
    
    // 测试连接是否有效
    try {
        if (!TestConnection(*conn)) {
            spdlog::warn("Connection test failed, creating new connection");
            conn = CreateConnection();
        }
    } catch (const std::exception& e) {
        spdlog::error("Error testing connection: {}", e.what());
        conn = CreateConnection();
    }
    
    // 添加到活跃连接集合
    active_connections_.insert(conn);
    
    return conn;
}

Task<std::shared_ptr<pqxx::connection>> ConnectionPool::GetConnectionAsync() {
    // 这里使用协程实现异步获取连接
    std::shared_ptr<pqxx::connection> conn;
    
    try {
        // 标记挂起请求
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_++;
        }
        
        // 尝试获取连接
        bool got_connection = false;
        while (!got_connection && !shutdown_) {
            {
                std::unique_lock<std::mutex> lock(mutex_);
                
                if (!idle_connections_.empty()) {
                    // 有空闲连接可用
                    conn = idle_connections_.front();
                    idle_connections_.pop_front();
                    active_connections_.insert(conn);
                    got_connection = true;
                } else if (active_connections_.size() < max_connections_) {
                    // 创建新连接
                    conn = CreateConnection();
                    active_connections_.insert(conn);
                    got_connection = true;
                } else {
                    // 等待通知
                    auto wait_result = condition_.wait_for(lock, std::chrono::milliseconds(100));
                    if (wait_result == std::cv_status::timeout) {
                        // 释放锁一小段时间，避免独占锁
                        lock.unlock();
                        co_await std::suspend_always{};
                    }
                }
            }
        }
        
        // 减少挂起请求计数
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_--;
        }
        
        if (shutdown_) {
            throw std::runtime_error("Connection pool is shutting down");
        }
        
        // 测试连接是否有效
        if (!TestConnection(*conn)) {
            spdlog::warn("Connection test failed, creating new connection");
            
            std::lock_guard<std::mutex> lock(mutex_);
            active_connections_.erase(conn);
            conn = CreateConnection();
            active_connections_.insert(conn);
        }
        
        co_return conn;
    } catch (const std::exception& e) {
        // 确保减少挂起请求计数
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_--;
        }
        
        spdlog::error("Error in GetConnectionAsync: {}", e.what());
        throw;
    }
}

void ConnectionPool::ReleaseConnection(std::shared_ptr<pqxx::connection> connection) {
    if (!connection) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 从活跃连接中移除
    auto it = active_connections_.find(connection);
    if (it != active_connections_.end()) {
        active_connections_.erase(it);
        
        // 检查连接是否仍然有效
        try {
            if (TestConnection(*connection)) {
                // 将连接返回空闲池
                idle_connections_.push_back(connection);
                condition_.notify_one();
            }
            // 如果连接无效，它会在这个函数结束时被销毁
        } catch (const std::exception& e) {
            spdlog::error("Error testing connection during release: {}", e.what());
            // 连接无效，让它被销毁
        }
    }
}

void ConnectionPool::CloseAll() {
    // 设置关闭标志
    shutdown_ = true;
    condition_.notify_all();
    
    // 等待监控线程结束
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 清空空闲连接
    idle_connections_.clear();
    
    // 清空活跃连接
    active_connections_.clear();
    
    spdlog::info("Database connection pool shut down");
}

std::shared_ptr<pqxx::connection> ConnectionPool::CreateConnection() {
    try {
        auto conn = std::make_shared<pqxx::connection>(connection_string_);
        return conn;
    } catch (const std::exception& e) {
        spdlog::error("Failed to create database connection: {}", e.what());
        throw;
    }
}

void ConnectionPool::MonitorConnections() {
    while (!shutdown_) {
        std::this_thread::sleep_for(std::chrono::seconds(30)); // 每30秒检查一次
        
        std::unique_lock<std::mutex> lock(mutex_);
        
        // 计算需要保留的最小空闲连接数
        size_t min_idle = min_connections_;
        
        // 如果有挂起的请求，保留更多空闲连接
        if (pending_requests_ > 0) {
            min_idle = std::min(min_connections_ + pending_requests_.load(), max_connections_);
        }
        
        // 移除过多的空闲连接
        auto now = std::chrono::steady_clock::now();
        
        // 移除超时的空闲连接，但确保至少保留最小数量
        while (idle_connections_.size() > min_idle) {
            auto conn = idle_connections_.back();
            idle_connections_.pop_back();
            
            // 连接会在离开作用域时自动关闭
        }
        
        // 记录连接池状态
        spdlog::debug("Connection pool stats - Active: {}, Idle: {}, Pending: {}",
                   active_connections_.size(), idle_connections_.size(), pending_requests_.load());
    }
}

bool ConnectionPool::TestConnection(pqxx::connection& conn) {
    try {
        // 简单检查连接是否仍然有效
        if (!conn.is_open()) {
            return false;
        }
        
        // 执行简单查询
        pqxx::work txn(conn);
        txn.exec1("SELECT 1");
        txn.commit();
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Connection test failed: {}", e.what());
        return false;
    }
}

ConnectionPool::PoolStats ConnectionPool::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PoolStats stats;
    stats.active_connections = active_connections_.size();
    stats.idle_connections = idle_connections_.size();
    stats.pending_requests = pending_requests_.load();
    
    return stats;
}

} // namespace ai_backend::core::db