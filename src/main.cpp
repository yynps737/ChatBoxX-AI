#include <iostream>
#include <signal.h>
#include <spdlog/spdlog.h>

#include "core/config/config_manager.h"
#include "core/http/http_server.h"
#include "core/http/router.h"
#include "core/db/connection_pool.h"
#include "api/routes/api_router.h"
#include "services/ai/model_service.h"

// 全局HTTP服务器指针，用于信号处理
std::shared_ptr<ai_backend::core::http::HttpServer> g_http_server;

// 信号处理函数
void SignalHandler(int signal) {
    spdlog::info("Received signal {}, shutting down...", signal);
    if (g_http_server) {
        g_http_server->Stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    try {
        // 设置日志
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        spdlog::set_level(spdlog::level::info);
        
        spdlog::info("Starting AI Backend Service");
        
        // 加载配置
        auto& config = ai_backend::core::config::ConfigManager::GetInstance();
        std::string config_path = "config/config.toml";
        
        // 如果命令行提供了配置文件路径，则使用它
        if (argc > 1) {
            config_path = argv[1];
        }
        
        if (!config.LoadFromFile(config_path)) {
            spdlog::critical("Failed to load configuration from {}", config_path);
            return 1;
        }
        
        spdlog::info("Configuration loaded from {}", config_path);
        
        // 设置日志级别
        std::string log_level = config.GetString("log.level", "info");
        if (log_level == "trace") {
            spdlog::set_level(spdlog::level::trace);
        } else if (log_level == "debug") {
            spdlog::set_level(spdlog::level::debug);
        } else if (log_level == "info") {
            spdlog::set_level(spdlog::level::info);
        } else if (log_level == "warn") {
            spdlog::set_level(spdlog::level::warn);
        } else if (log_level == "error") {
            spdlog::set_level(spdlog::level::err);
        } else if (log_level == "critical") {
            spdlog::set_level(spdlog::level::critical);
        }
        
        // 初始化数据库连接池
        std::string db_connection_string = config.GetString("database.connection_string", "");
        if (db_connection_string.empty()) {
            spdlog::critical("Database connection string not configured");
            return 1;
        }
        
        auto& db_pool = ai_backend::core::db::ConnectionPool::GetInstance();
        if (!db_pool.Initialize(db_connection_string, 
                              config.GetInt("database.min_connections", 5),
                              config.GetInt("database.max_connections", 20))) {
            spdlog::critical("Failed to initialize database connection pool");
            return 1;
        }
        
        spdlog::info("Database connection pool initialized");
        
        // 初始化模型服务
        ai_backend::services::ai::ModelService::GetInstance().Initialize();
        spdlog::info("AI Model service initialized");
        
        // 创建API路由器
        auto router = std::make_shared<ai_backend::api::routes::ApiRouter>();
        router->Initialize();
        spdlog::info("API router initialized");
        
        // 创建HTTP服务器
        uint16_t port = static_cast<uint16_t>(config.GetInt("server.port", 8080));
        size_t thread_count = static_cast<size_t>(config.GetInt("server.threads", 0));
        
        g_http_server = std::make_shared<ai_backend::core::http::HttpServer>(port, thread_count);
        g_http_server->SetRouter(router);
        
        // 设置信号处理
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);
        
        // 启动服务器
        spdlog::info("Starting HTTP server on port {}", port);
        g_http_server->Start();
        
        // 主线程等待，实际工作由工作线程池处理
        std::string cmd;
        spdlog::info("Server running. Press 'q' to quit.");
        while (std::cin >> cmd) {
            if (cmd == "q" || cmd == "quit" || cmd == "exit") {
                break;
            }
        }
        
        // 停止服务器
        spdlog::info("Shutting down server...");
        g_http_server->Stop();
        
        spdlog::info("Server shutdown complete");
        return 0;
    } catch (const std::exception& e) {
        spdlog::critical("Unhandled exception: {}", e.what());
        return 1;
    }
}