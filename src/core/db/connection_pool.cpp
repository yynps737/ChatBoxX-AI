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

void ConnectionPool::MonitorConnections() {
    while (!shutdown_) {
        std::this_thread::sleep_for(std::chrono::seconds(30)); // 每30秒检查一次
        
        try {
            std::unique_lock<std::mutex> lock(mutex_);
            
            // 计算需要保留的最小空闲连接数
            size_t min_idle = min_connections_;
            
            // 如果有挂起的请求，保留更多空闲连接
            if (pending_requests_ > 0) {
                min_idle = std::min(min_connections_ + pending_requests_.load(), max_connections_);
            }
            
            // 移除过多的空闲连接
            auto now = std::chrono::steady_clock::now();
            
            // 检查并关闭不活跃连接
            for (auto it = idle_connections_.begin(); it != idle_connections_.end(); ) {
                if (idle_connections_.size() <= min_idle) {
                    break;
                }
                
                // 测试连接是否仍然有效
                bool is_valid = false;
                try {
                    is_valid = TestConnection(*(*it));
                } catch (...) {
                    is_valid = false;
                }
                
                if (!is_valid) {
                    it = idle_connections_.erase(it);
                } else {
                    ++it;
                }
            }
            
            // 确保至少有最小数量的连接
            while (idle_connections_.size() < min_idle) {
                try {
                    auto conn = CreateConnection();
                    idle_connections_.push_back(conn);
                } catch (const std::exception& e) {
                    spdlog::error("Failed to create connection during monitoring: {}", e.what());
                    break;
                }
            }
            
            // 记录连接池状态
            spdlog::debug("Connection pool stats - Active: {}, Idle: {}, Pending: {}",
                       active_connections_.size(), idle_connections_.size(), pending_requests_.load());
        } catch (const std::exception& e) {
            spdlog::error("Error in connection pool monitoring: {}", e.what());
        }
    }
}

bool ConnectionPool::TestConnection(pqxx::connection& conn) {
    try {
        // 简单检查连接是否仍然有效
        if (!conn.is_open()) {
            return false;
        }
        
        // 执行简单查询并设置超时
        pqxx::work txn(conn);
        txn.set_variable("statement_timeout", "1000"); // 1秒超时
        txn.exec1("SELECT 1");
        txn.commit();
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Connection test failed: {}", e.what());
        try {
            // 尝试重新连接
            conn.disconnect();
            conn.activate();
            
            // 再次测试
            pqxx::work txn(conn);
            txn.exec1("SELECT 1");
            txn.commit();
            
            spdlog::info("Connection recovered after disconnect/activate");
            return true;
        } catch (...) {
            return false;
        }
    }
}
}