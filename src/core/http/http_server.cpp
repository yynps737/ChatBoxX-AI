#include "core/http/http_server.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::http {

using namespace async;

HttpServer::HttpServer(uint16_t port, size_t thread_count)
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

HttpServer::~HttpServer() {
    Stop();
}

void HttpServer::Start() {
    if (running_) {
        return;
    }
    
    try {
        // 创建工作守卫，防止io_context在没有任务时退出
        work_guard_.emplace(net::make_work_guard(io_context_));
        
        // 配置接受器
        tcp::endpoint endpoint(tcp::v4(), port_);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(net::socket_base::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen(net::socket_base::max_listen_connections);
        
        // 启动工作线程
        worker_threads_.reserve(thread_count_);
        for (size_t i = 0; i < thread_count_; ++i) {
            worker_threads_.emplace_back([this]() {
                try {
                    io_context_.run();
                } catch (const std::exception& e) {
                    spdlog::error("Exception in HTTP server worker thread: {}", e.what());
                }
            });
        }
        
        running_ = true;
        
        // 开始接受连接
        AcceptConnection();
        
        spdlog::info("HTTP server started on port {}", port_);
    } catch (const std::exception& e) {
        spdlog::error("Failed to start HTTP server: {}", e.what());
        Stop();
        throw;
    }
}

void HttpServer::Stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // 关闭接受器
    beast::error_code ec;
    acceptor_.close(ec);
    
    // 移除工作守卫，允许io_context退出
    if (work_guard_) {
        work_guard_.reset();
    }
    
    // 停止io_context
    io_context_.stop();
    
    // 等待所有工作线程结束
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    
    spdlog::info("HTTP server stopped");
}

void HttpServer::SetRouter(std::shared_ptr<Router> router) {
    router_ = router;
}

size_t HttpServer::GetActiveConnections() const {
    return active_connections_;
}

uint16_t HttpServer::GetPort() const {
    return port_;
}

bool HttpServer::IsRunning() const {
    return running_;
}

void HttpServer::AcceptConnection() {
    // 确保服务器正在运行
    if (!running_) {
        return;
    }
    
    acceptor_.async_accept(
        [self = shared_from_this()](beast::error_code ec, tcp::socket socket) {
            if (ec) {
                if (ec != net::error::operation_aborted && self->running_) {
                    spdlog::error("Accept error: {}", ec.message());
                }
            } else {
                self->active_connections_++;
                
                // 创建新会话
                auto session = std::make_shared<HttpSession>(std::move(socket), self->router_);
                session->Start();
            }
            
            // 继续接受下一个连接
            if (self->running_) {
                self->AcceptConnection();
            }
        });
}

// HttpSession实现
HttpServer::HttpSession::HttpSession(tcp::socket&& socket, std::shared_ptr<Router> router)
    : socket_(std::move(socket)),
      router_(router),
      close_connection_(false) {
}

void HttpServer::HttpSession::Start() {
    ReadRequest();
}

void HttpServer::HttpSession::ReadRequest() {
    auto self = shared_from_this();
    
    // 设置请求解析器超时
    socket_.expires_after(std::chrono::seconds(30));
    
    // 异步读取请求
    http::async_read(socket_, buffer_, request_,
        [self](beast::error_code ec, std::size_t bytes_transferred) {
            boost::ignore_unused(bytes_transferred);
            
            if (ec) {
                return self->HandleError(ec, "read");
            }
            
            // 处理请求
            self->ProcessRequest();
        });
}

Task<void> HttpServer::HttpSession::ProcessRequest() {
    try {
        // 转换成我们自己的Request对象
        Request req;
        req.method = std::string(request_.method_string());
        req.target = std::string(request_.target());
        req.version = request_.version();
        
        // 复制头部
        for (auto const& field : request_) {
            req.headers.emplace(
                std::string(field.name_string()),
                std::string(field.value())
            );
        }
        
        // 复制body
        req.body = request_.body();
        
        // 解析查询参数
        if (req.target.find('?') != std::string::npos) {
            auto pos = req.target.find('?');
            std::string query_string = req.target.substr(pos + 1);
            req.path = req.target.substr(0, pos);
            
            // 简单解析查询参数
            std::istringstream stream(query_string);
            std::string pair;
            while (std::getline(stream, pair, '&')) {
                auto eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = pair.substr(0, eq_pos);
                    std::string value = pair.substr(eq_pos + 1);
                    req.query_params[key] = value;
                } else {
                    req.query_params[pair] = "";
                }
            }
        } else {
            req.path = req.target;
        }
        
        // 确保路径始终以/开头
        if (req.path.empty() || req.path[0] != '/') {
            req.path = "/" + req.path;
        }
        
        // 记录请求
        spdlog::info("HTTP {} {}", req.method, req.path);
        
        Response resp;
        
        // 通过路由器处理请求
        if (router_) {
            resp = co_await router_->Route(req);
        } else {
            // 如果没有路由器，返回404
            resp.status_code = 404;
            resp.body = "{\"error\":\"Not Found\"}";
            resp.headers["Content-Type"] = "application/json";
        }
        
        // 发送响应
        WriteResponse(resp);
    } catch (const std::exception& e) {
        spdlog::error("Exception in ProcessRequest: {}", e.what());
        
        // 返回500错误
        Response error_resp;
        error_resp.status_code = 500;
        error_resp.body = "{\"error\":\"Internal Server Error\"}";
        error_resp.headers["Content-Type"] = "application/json";
        WriteResponse(error_resp);
    }
}

void HttpServer::HttpSession::WriteResponse(const Response& resp) {
    auto self = shared_from_this();
    
    // 转换到Beast的响应格式
    response_.version(request_.version());
    response_.result(resp.status_code);
    response_.set(http::field::server, "AiBackend");
    
    // 设置内容长度
    response_.set(http::field::content_length, std::to_string(resp.body.size()));
    
    // 设置其他头部
    for (const auto& [name, value] : resp.headers) {
        response_.set(name, value);
    }
    
    // 设置Keep-Alive头部
    bool should_close = close_connection_ || resp.headers.count("Connection") > 0 && 
                        resp.headers.at("Connection") == "close";
    
    if (should_close) {
        response_.set(http::field::connection, "close");
    } else {
        response_.set(http::field::connection, "keep-alive");
    }
    
    // 设置响应体
    response_.body() = resp.body;
    
    // 发送响应
    http::async_write(socket_, response_,
        [self, should_close](beast::error_code ec, std::size_t) {
            if (ec) {
                return self->HandleError(ec, "write");
            }
            
            if (should_close) {
                // 关闭连接
                return self->Close();
            }
            
            // 清理旧请求数据
            self->request_ = {};
            
            // 读取下一个请求
            self->ReadRequest();
        });
}

void HttpServer::HttpSession::Close() {
    beast::error_code ec;
    socket_.shutdown(tcp::socket::shutdown_send, ec);
}

void HttpServer::HttpSession::HandleError(beast::error_code ec, const char* what) {
    if (ec == http::error::end_of_stream) {
        return Close();
    }
    
    if (ec == net::error::operation_aborted) {
        return;
    }
    
    spdlog::error("HttpSession {} error: {}", what, ec.message());
}

} // namespace ai_backend::core::http