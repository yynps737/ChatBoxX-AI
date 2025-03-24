#include "core/websocket/websocket_session.h"
#include "core/utils/uuid.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::websocket {

WebSocketSession::WebSocketSession(
    tcp::socket&& socket,
    MessageHandler message_handler,
    ConnectionHandler connection_handler,
    CloseHandler close_handler,
    DisconnectHandler disconnect_handler)
    : ws_(std::move(socket)),
      message_handler_(std::move(message_handler)),
      connection_handler_(std::move(connection_handler)),
      close_handler_(std::move(close_handler)),
      disconnect_handler_(std::move(disconnect_handler)),
      client_id_(core::utils::UuidGenerator::GenerateUuid()),
      closed_(false) {
    
    ws_.set_option(
        websocket::stream_base::timeout::suggested(
            beast::role_type::server
        )
    );
    
    ws_.set_option(websocket::stream_base::decorator(
        [](websocket::response_type& res) {
            res.set(http::field::server, "AI Backend WebSocket Server");
        }
    ));
}

WebSocketSession::~WebSocketSession() {
    Close();
}

void WebSocketSession::Start() {
    ws_.async_accept(
        beast::bind_front_handler(
            &WebSocketSession::OnAccept,
            shared_from_this()
        )
    );
}

void WebSocketSession::OnAccept(beast::error_code ec) {
    if (ec) {
        return HandleError(ec, "accept");
    }
    
    spdlog::info("WebSocket connection established: {}", client_id_);
    
    if (connection_handler_) {
        connection_handler_(client_id_);
    }
    
    ReadMessage();
}

void WebSocketSession::ReadMessage() {
    if (closed_) {
        return;
    }
    
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &WebSocketSession::OnRead,
            shared_from_this()
        )
    );
}

void WebSocketSession::OnRead(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec == websocket::error::closed) {
        HandleClose();
        return;
    }
    
    if (ec) {
        return HandleError(ec, "read");
    }
    
    std::string message = beast::buffers_to_string(buffer_.data());
    buffer_.consume(buffer_.size());
    
    spdlog::debug("WebSocket message received from {}: {}", client_id_, message);
    
    if (message_handler_) {
        message_handler_(client_id_, message);
    }
    
    ReadMessage();
}

bool WebSocketSession::Send(const std::string& message) {
    if (closed_) {
        return false;
    }
    
    try {
        std::lock_guard<std::mutex> lock(write_mutex_);
        ws_.write(net::buffer(message));
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to send WebSocket message: {}", e.what());
        return false;
    }
}

void WebSocketSession::AsyncSend(const std::string& message) {
    if (closed_) {
        return;
    }
    
    auto self = shared_from_this();
    
    net::post(
        ws_.get_executor(),
        [self, message]() {
            std::lock_guard<std::mutex> lock(self->write_mutex_);
            self->outgoing_messages_.push_back(message);
            
            if (self->outgoing_messages_.size() == 1) {
                self->WriteMessage();
            }
        }
    );
}

void WebSocketSession::WriteMessage() {
    if (closed_ || outgoing_messages_.empty()) {
        return;
    }
    
    auto self = shared_from_this();
    
    ws_.async_write(
        net::buffer(outgoing_messages_.front()),
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            self->OnWrite(ec, bytes_transferred);
        }
    );
}

void WebSocketSession::OnWrite(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec) {
        return HandleError(ec, "write");
    }
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    outgoing_messages_.pop_front();
    
    if (!outgoing_messages_.empty()) {
        WriteMessage();
    }
}

void WebSocketSession::Close() {
    if (closed_) {
        return;
    }
    
    closed_ = true;
    
    beast::error_code ec;
    ws_.close(websocket::close_code::normal, ec);
    
    if (ec) {
        spdlog::error("Error closing WebSocket connection: {}", ec.message());
    }
    
    if (disconnect_handler_) {
        disconnect_handler_(client_id_);
    }
}

void WebSocketSession::HandleClose() {
    if (closed_) {
        return;
    }
    
    closed_ = true;
    
    spdlog::info("WebSocket connection closed: {}", client_id_);
    
    if (close_handler_) {
        close_handler_(client_id_);
    }
    
    if (disconnect_handler_) {
        disconnect_handler_(client_id_);
    }
}

void WebSocketSession::HandleError(beast::error_code ec, const char* what) {
    if (closed_) {
        return;
    }
    
    spdlog::error("WebSocket {} error: {}", what, ec.message());
    
    Close();
}

std::string WebSocketSession::GetClientId() const {
    return client_id_;
}

bool WebSocketSession::IsClosed() const {
    return closed_;
}

} // namespace ai_backend::core::websocket