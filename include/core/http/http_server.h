#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "request.h"
#include "response.h"
#include "router.h"
#include "core/async/task.h"

namespace ai_backend::core::http {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// HTTP服务器类，处理HTTP请求
class HttpServer : public std::enable_shared_from_this<HttpServer> {
public:
    HttpServer(uint16_t port, size_t thread_count = 0);
    ~HttpServer();

    // 启动服务器
    void Start();
    
    // 停止服务器
    void Stop();
    
    // 设置路由器
    void SetRouter(std::shared_ptr<Router> router);
    
    // 获取当前连接数
    size_t GetActiveConnections() const;
    
    // 获取端口号
    uint16_t GetPort() const;
    
    // 检查服务器是否正在运行
    bool IsRunning() const;

private:
    // 接受新连接
    void AcceptConnection();
    
    // 处理HTTP会话
    class HttpSession : public std::enable_shared_from_this<HttpSession> {
    public:
        HttpSession(tcp::socket&& socket, std::shared_ptr<Router> router);
        
        // 启动会话
        void Start();
        
    private:
        // 读取请求
        void ReadRequest();
        
        // 处理完整请求
        async::Task<void> ProcessRequest();
        
        // 写入响应
        void WriteResponse(const Response& response);
        
        // 关闭连接
        void Close();
        
        // 处理错误
        void HandleError(beast::error_code ec, const char* what);

    private:
        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;
        http::response<http::string_body> response_;
        std::shared_ptr<Router> router_;
        std::atomic<bool> close_connection_;
    };

private:
    uint16_t port_;
    size_t thread_count_;
    std::atomic<bool> running_;
    std::atomic<size_t> active_connections_;
    
    net::io_context io_context_;
    std::optional<net::executor_work_guard<net::io_context::executor_type>> work_guard_;
    tcp::acceptor acceptor_;
    std::vector<std::thread> worker_threads_;
    std::shared_ptr<Router> router_;
};

} // namespace ai_backend::core::http