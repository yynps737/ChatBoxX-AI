#include "services/dialog/dialog_service.h"
#include "core/utils/uuid.h"
#include "core/db/connection_pool.h"
#include <spdlog/spdlog.h>
#include <pqxx/pqxx>

namespace ai_backend::services::dialog {

using namespace core::async;

DialogService::DialogService() {
}

Task<common::Result<models::Dialog>> DialogService::GetDialogById(const std::string& dialog_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, user_id, title, model_id, is_archived, created_at, updated_at, "
            "(SELECT content FROM messages WHERE dialog_id = d.id ORDER BY created_at DESC LIMIT 1) as last_message "
            "FROM dialogs d WHERE id = $1",
            dialog_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<models::Dialog>::Error("对话不存在");
        }
        
        models::Dialog dialog;
        dialog.id = result[0][0].as<std::string>();
        dialog.user_id = result[0][1].as<std::string>();
        dialog.title = result[0][2].as<std::string>();
        dialog.model_id = result[0][3].as<std::string>();
        dialog.is_archived = result[0][4].as<bool>();
        dialog.created_at = result[0][5].as<std::string>();
        dialog.updated_at = result[0][6].as<std::string>();
        
        if (!result[0][7].is_null()) {
            dialog.last_message = result[0][7].as<std::string>();
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<models::Dialog>::Ok(dialog);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetDialogById: {}", e.what());
        co_return common::Result<models::Dialog>::Error("获取对话失败");
    }
}

Task<common::Result<std::vector<models::Dialog>>> 
DialogService::GetDialogsByUserId(const std::string& user_id, int page, int page_size, bool include_archived) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        
        std::string query =
            "SELECT id, user_id, title, model_id, is_archived, created_at, updated_at, "
            "(SELECT content FROM messages WHERE dialog_id = d.id ORDER BY created_at DESC LIMIT 1) as last_message "
            "FROM dialogs d WHERE user_id = $1";
        
        if (!include_archived) {
            query += " AND is_archived = false";
        }
        
        query += " ORDER BY updated_at DESC LIMIT $2 OFFSET $3";
        
        int offset = (page - 1) * page_size;
        auto result = txn.exec_params(
            query,
            user_id, page_size, offset
        );
        
        std::vector<models::Dialog> dialogs;
        for (const auto& row : result) {
            models::Dialog dialog;
            dialog.id = row[0].as<std::string>();
            dialog.user_id = row[1].as<std::string>();
            dialog.title = row[2].as<std::string>();
            dialog.model_id = row[3].as<std::string>();
            dialog.is_archived = row[4].as<bool>();
            dialog.created_at = row[5].as<std::string>();
            dialog.updated_at = row[6].as<std::string>();
            
            if (!row[7].is_null()) {
                dialog.last_message = row[7].as<std::string>();
            }
            
            dialogs.push_back(dialog);
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::Dialog>>::Ok(dialogs);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetDialogsByUserId: {}", e.what());
        co_return common::Result<std::vector<models::Dialog>>::Error("获取对话列表失败");
    }
}

Task<common::Result<models::Dialog>> DialogService::CreateDialog(const models::Dialog& dialog) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        // 生成UUID
        std::string dialog_id = core::utils::UuidGenerator::GenerateUuid();
        
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO dialogs (id, user_id, title, model_id, is_archived, created_at, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, NOW(), NOW())",
            dialog_id, dialog.user_id, dialog.title, dialog.model_id, dialog.is_archived
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        // 获取新创建的对话
        auto result = co_await GetDialogById(dialog_id);
        
        co_return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in CreateDialog: {}", e.what());
        co_return common::Result<models::Dialog>::Error("创建对话失败");
    }
}

Task<common::Result<models::Dialog>> DialogService::UpdateDialog(const models::Dialog& dialog) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        txn.exec_params(
            "UPDATE dialogs SET title = $1, is_archived = $2, updated_at = NOW() "
            "WHERE id = $3",
            dialog.title, dialog.is_archived, dialog.id
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        // 获取更新后的对话
        auto result = co_await GetDialogById(dialog.id);
        
        co_return result;
    } catch (const std::exception& e) {
        spdlog::error("Error in UpdateDialog: {}", e.what());
        co_return common::Result<models::Dialog>::Error("更新对话失败");
    }
}

Task<common::Result<void>> DialogService::DeleteDialog(const std::string& dialog_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        
        // 先删除对话中的所有消息
        txn.exec_params(
            "DELETE FROM messages WHERE dialog_id = $1",
            dialog_id
        );
        
        // 然后删除对话
        txn.exec_params(
            "DELETE FROM dialogs WHERE id = $1",
            dialog_id
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteDialog: {}", e.what());
        co_return common::Result<void>::Error("删除对话失败");
    }
}

Task<common::Result<std::string>> 
DialogService::ValidateDialogOwnership(const std::string& dialog_id, const std::string& user_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT user_id FROM dialogs WHERE id = $1",
            dialog_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<std::string>::Error("对话不存在");
        }
        
        std::string owner_id = result[0][0].as<std::string>();
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        if (owner_id != user_id) {
            co_return common::Result<std::string>::Error("无权访问此对话");
        }
        
        co_return common::Result<std::string>::Ok(owner_id);
    } catch (const std::exception& e) {
        spdlog::error("Error in ValidateDialogOwnership: {}", e.what());
        co_return common::Result<std::string>::Error("验证对话所有权失败");
    }
}

} // namespace ai_backend::services::dialog