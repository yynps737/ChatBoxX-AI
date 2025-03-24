#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace ai_backend::core::websocket {

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    using MessageHandler = std::function<void(const std::string&, const std::string&)>;
    using ConnectionHandler = std::function<void(const std::string&)>;
    using CloseHandler = std::function<void(const std::string&)>;
    using DisconnectHandler = std::function<void(const std::string&)>;
    
    WebSocketSession(
        tcp::socket&& socket,
        MessageHandler message_handler,
        ConnectionHandler connection_handler,
        CloseHandler close_handler,
        DisconnectHandler disconnect_handler
    );
    
    ~WebSocketSession();
    
    void Start();
    void Close();
    
    bool Send(const std::string& message);
    void AsyncSend(const std::string& message);
    
    std::string GetClientId() const;
    bool IsClosed() const;
    
private:
    void OnAccept(beast::error_code ec);
    void ReadMessage();
    void OnRead(beast::error_code ec, std::size_t bytes_transferred);
    void WriteMessage();
    void OnWrite(beast::error_code ec, std::size_t bytes_transferred);
    void HandleClose();
    void HandleError(beast::error_code ec, const char* what);
    
private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;
    
    MessageHandler message_handler_;
    ConnectionHandler connection_handler_;
    CloseHandler close_handler_;
    DisconnectHandler disconnect_handler_;
    
    std::mutex write_mutex_;
    std::deque<std::string> outgoing_messages_;
    
    std::string client_id_;
    std::atomic<bool> closed_;
};

} // namespace ai_backend::core::websocket