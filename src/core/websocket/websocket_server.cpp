#include "core/websocket/websocket_server.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::websocket {

WebSocketServer::WebSocketServer(uint16_t port, size_t thread_count)
    : port_(port),
      thread_count_(thread_count),
      running_(false),
      active_connections_(0),
      io_context_(),
      acceptor_(io_context_) {
    
    if (thread_count_ == 0) {
        thread_count_ = std::thread::hardware_concurrency();
    }
}

WebSocketServer::~WebSocketServer() {
    Stop();
}

void WebSocketServer::Start() {
    if (running_) {
        return;
    }
    
    try {
        work_guard_.emplace(net::make_work_guard(io_context_));
        
        tcp::endpoint endpoint(tcp::v4(), port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
        
        worker_threads_.reserve(thread_count_);
        for (size_t i = 0; i < thread_count_; ++i) {
            worker_threads_.emplace_back([this]() {
                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    spdlog::error("Exception in WebSocket server worker thread: {}", e.what());
                }
            });
        }
        
        running_ = true;
        
        AcceptConnection();
        
        spdlog::info("WebSocket server started on port {}", port_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to start WebSocket server: {}", e.what());
        Stop();
        throw;
    }
}

void WebSocketServer::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    beast::error_code ec;
    acceptor_.close(ec);
    
    if (work_guard_) {
        work_guard_.reset();
    }
    
    io_context_.stop();
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    
    for (auto& session : sessions_) {
        session->Close();
    }
    
    sessions_.clear();
    
    spdlog::info("WebSocket server stopped");
}

void WebSocketServer::SetMessageHandler(MessageHandler handler) {
    message_handler_ = std::move(handler);
}

void WebSocketServer::SetConnectionHandler(ConnectionHandler handler) {
    connection_handler_ = std::move(handler);
}

void WebSocketServer::SetCloseHandler(CloseHandler handler) {
    close_handler_ = std::move(handler);
}

size_t WebSocketServer::GetActiveConnections() const {
    return active_connections_;
}

bool WebSocketServer::Broadcast(const std::string& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : sessions_) {
        try {
            session->Send(message);
        } catch (const std::exception& e) {
            spdlog::error("Failed to broadcast message to session: {}", e.what());
        }
    }
    return true;
}

bool WebSocketServer::SendTo(const std::string& client_id, const std::string& message) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& session : sessions_) {
        if (session->GetClientId() == client_id) {
            return session->Send(message);
        }
    }
    return false;
}

void WebSocketServer::AcceptConnection() {
    if (!running_) {
        return;
    }
    
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (ec) {
                if (ec != net::error::operation_aborted && running_) {
                    spdlog::error("WebSocket accept error: {}", ec.message());
                }
            } else {
                active_connections_++;
                
                auto session = std::make_shared<WebSocketSession>(
                    std::move(socket), 
                    message_handler_,
                    connection_handler_,
                    close_handler_,
                    [this](const std::string& client_id) { RemoveSession(client_id); }
                );
                
                {
                    std::lock_guard<std::mutex> lock(sessions_mutex_);
                    sessions_.push_back(session);
                }
                
                session->Start();
            }
            
            if (running_) {
                AcceptConnection();
            }
        });
}

void WebSocketServer::RemoveSession(const std::string& client_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
            [&client_id](const std::shared_ptr<WebSocketSession>& session) {
                return session->GetClientId() == client_id;
            }),
        sessions_.end()
    );
    active_connections_--;
}

} // namespace ai_backend::core::websocket