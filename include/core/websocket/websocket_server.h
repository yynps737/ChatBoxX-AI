#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include "websocket_session.h"

namespace ai_backend::core::websocket {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class WebSocketServer : public std::enable_shared_from_this<WebSocketServer> {
public:
    using MessageHandler = std::function<void(const std::string&, const std::string&)>;
    using ConnectionHandler = std::function<void(const std::string&)>;
    using CloseHandler = std::function<void(const std::string&)>;
    using DisconnectHandler = std::function<void(const std::string&)>;
    
    WebSocketServer(uint16_t port, size_t thread_count = 0);
    ~WebSocketServer();
    
    void Start();
    void Stop();
    
    void SetMessageHandler(MessageHandler handler);
    void SetConnectionHandler(ConnectionHandler handler);
    void SetCloseHandler(CloseHandler handler);
    
    size_t GetActiveConnections() const;
    
    bool Broadcast(const std::string& message);
    bool SendTo(const std::string& client_id, const std::string& message);
    
private:
    void AcceptConnection();
    void RemoveSession(const std::string& client_id);
    
private:
    uint16_t port_;
    size_t thread_count_;
    std::atomic<bool> running_;
    std::atomic<size_t> active_connections_;
    
    net::io_context io_context_;
    std::optional<net::executor_work_guard<net::io_context::executor_type>> work_guard_;
    tcp::acceptor acceptor_;
    std::vector<std::thread> worker_threads_;
    
    std::mutex sessions_mutex_;
    std::vector<std::shared_ptr<WebSocketSession>> sessions_;
    
    MessageHandler message_handler_;
    ConnectionHandler connection_handler_;
    CloseHandler close_handler_;
};

} // namespace ai_backend::core::websocket