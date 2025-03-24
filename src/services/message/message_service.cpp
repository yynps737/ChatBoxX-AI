#include "services/message/message_service.h"
#include "core/utils/uuid.h"
#include "core/db/connection_pool.h"
#include <spdlog/spdlog.h>
#include <pqxx/pqxx>

namespace ai_backend::services::message {

using namespace core::async;

MessageService::MessageService() {
}

Task<common::Result<models::Message>> 
MessageService::GetMessageById(const std::string& message_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, dialog_id, role, content, type, tokens, created_at "
            "FROM messages WHERE id = $1",
            message_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<models::Message>::Error("消息不存在");
        }
        
        models::Message message;
        message.id = result[0][0].as<std::string>();
        message.dialog_id = result[0][1].as<std::string>();
        message.role = result[0][2].as<std::string>();
        message.content = result[0][3].as<std::string>();
        message.type = result[0][4].as<std::string>();
        message.tokens = result[0][5].as<size_t>();
        message.created_at = result[0][6].as<std::string>();
        
        // 获取附件
        auto file_result = txn.exec_params(
            "SELECT id, name, type, url FROM files WHERE message_id = $1",
            message_id
        );
        
        for (const auto& row : file_result) {
            models::Attachment attachment;
            attachment.id = row[0].as<std::string>();
            attachment.name = row[1].as<std::string>();
            attachment.type = row[2].as<std::string>();
            attachment.url = row[3].as<std::string>();
            
            message.attachments.push_back(attachment);
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<models::Message>::Ok(message);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetMessageById: {}", e.what());
        co_return common::Result<models::Message>::Error("获取消息失败");
    }
}

Task<common::Result<std::vector<models::Message>>> 
MessageService::GetMessagesByDialogId(const std::string& dialog_id, int page, int page_size) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        
        int offset = (page - 1) * page_size;
        auto result = txn.exec_params(
            "SELECT id, dialog_id, role, content, type, tokens, created_at "
            "FROM messages WHERE dialog_id = $1 "
            "ORDER BY created_at DESC LIMIT $2 OFFSET $3",
            dialog_id, page_size, offset
        );
        
        std::vector<models::Message> messages;
        for (const auto& row : result) {
            models::Message message;
            message.id = row[0].as<std::string>();
            message.dialog_id = row[1].as<std::string>();
            message.role = row[2].as<std::string>();
            message.content = row[3].as<std::string>();
            message.type = row[4].as<std::string>();
            message.tokens = row[5].as<size_t>();
            message.created_at = row[6].as<std::string>();
            
            messages.push_back(message);
        }
        
        // 获取所有消息的附件
        for (auto& message : messages) {
            auto file_result = txn.exec_params(
                "SELECT id, name, type, url FROM files WHERE message_id = $1",
                message.id
            );
            
            for (const auto& row : file_result) {
                models::Attachment attachment;
                attachment.id = row[0].as<std::string>();
                attachment.name = row[1].as<std::string>();
                attachment.type = row[2].as<std::string>();
                attachment.url = row[3].as<std::string>();
                
                message.attachments.push_back(attachment);
            }
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::Message>>::Ok(messages);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetMessagesByDialogId: {}", e.what());
        co_return common::Result<std::vector<models::Message>>::Error("获取消息列表失败");
    }
}

Task<common::Result<std::vector<models::Message>>> 
MessageService::GetAllMessagesByDialogId(const std::string& dialog_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, dialog_id, role, content, type, tokens, created_at "
            "FROM messages WHERE dialog_id = $1 "
            "ORDER BY created_at",
            dialog_id
        );
        
        std::vector<models::Message> messages;
        for (const auto& row : result) {
            models::Message message;
            message.id = row[0].as<std::string>();
            message.dialog_id = row[1].as<std::string>();
            message.role = row[2].as<std::string>();
            message.content = row[3].as<std::string>();
            message.type = row[4].as<std::string>();
            message.tokens = row[5].as<size_t>();
            message.created_at = row[6].as<std::string>();
            
            messages.push_back(message);
        }
        
        // 获取所有消息的附件
        for (auto& message : messages) {
            auto file_result = txn.exec_params(
                "SELECT id, name, type, url FROM files WHERE message_id = $1",
                message.id
            );
            
            for (const auto& row : file_result) {
                models::Attachment attachment;
                attachment.id = row[0].as<std::string>();
                attachment.name = row[1].as<std::string>();
                attachment.type = row[2].as<std::string>();
                attachment.url = row[3].as<std::string>();
                
                message.attachments.push_back(attachment);
            }
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::Message>>::Ok(messages);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetAllMessagesByDialogId: {}", e.what());
        co_return common::Result<std::vector<models::Message>>::Error("获取消息列表失败");
    }
}

Task<common::Result<models::Message>> 
MessageService::CreateMessage(const models::Message& message) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        // 生成UUID
        std::string message_id = core::utils::UuidGenerator::GenerateUuid();
        
        pqxx::work txn(*conn);
        
        // 插入消息
        txn.exec_params(
            "INSERT INTO messages (id, dialog_id, role, content, type, tokens, created_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, NOW())",
            message_id, message.dialog_id, message.role, message.content, message.type, message.tokens
        );
        
        // 关联现有附件
        for (const auto& attachment : message.attachments) {
            txn.exec_params(
                "UPDATE files SET message_id = $1 WHERE id = $2",
                message_id, attachment.id
            );
        }
        
        // 更新对话的更新时间
        txn.exec_params(
            "UPDATE dialogs SET updated_at = NOW() WHERE id = $1",
            message.dialog_id
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        // 获取新创建的消息
        auto result = co_await GetMessageById(message_id);
        
        co_return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in CreateMessage: {}", e.what());
        co_return common::Result<models::Message>::Error("创建消息失败");
    }
}

Task<common::Result<void>> MessageService::DeleteMessage(const std::string& message_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        
        // 获取消息所属的对话ID
        auto dialog_result = txn.exec_params(
            "SELECT dialog_id FROM messages WHERE id = $1",
            message_id
        );
        
        if (dialog_result.empty()) {
            txn.abort();
            db_pool.ReleaseConnection(conn);
            co_return common::Result<void>::Error("消息不存在");
        }
        
        std::string dialog_id = dialog_result[0][0].as<std::string>();
        
        // 删除消息
        txn.exec_params(
            "DELETE FROM messages WHERE id = $1",
            message_id
        );
        
        // 更新对话的更新时间
        txn.exec_params(
            "UPDATE dialogs SET updated_at = NOW() WHERE id = $1",
            dialog_id
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteMessage: {}", e.what());
        co_return common::Result<void>::Error("删除消息失败");
    }
}

Task<common::Result<int>> MessageService::CountTokens(const std::string& content) {
    try {
        // 简单估算：按照4个字符约等于1个token
        int estimated_tokens = static_cast<int>(content.size() / 4);
        
        // 返回估算的token数量
        co_return common::Result<int>::Ok(estimated_tokens);
    } catch (const std::exception& e) {
        spdlog::error("Error in CountTokens: {}", e.what());
        co_return common::Result<int>::Error("计算token失败");
    }
}

} // namespace ai_backend::services::message