#include "core/db/prepared_statement.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::db {

PreparedStatement::PreparedStatement(std::shared_ptr<pqxx::connection> conn, const std::string& name, const std::string& sql)
    : conn_(conn), name_(name), sql_(sql) {
    try {
        if (!conn_->prepared(name_).exists()) {
            conn_->prepare(name_, sql_);
        }
    } catch (const std::exception& e) {
        spdlog::error("Failed to prepare statement {}: {}", name_, e.what());
        throw;
    }
}

std::string PreparedStatement::GetName() const {
    return name_;
}

std::string PreparedStatement::GetSQL() const {
    return sql_;
}

PreparedStatementCache::PreparedStatementCache(std::shared_ptr<pqxx::connection> conn)
    : conn_(conn) {
}

std::shared_ptr<PreparedStatement> PreparedStatementCache::GetOrCreate(const std::string& name, const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = statements_.find(name);
    if (it != statements_.end()) {
        return it->second;
    }
    
    auto statement = std::make_shared<PreparedStatement>(conn_, name, sql);
    statements_[name] = statement;
    return statement;
}

bool PreparedStatementCache::HasStatement(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return statements_.find(name) != statements_.end();
}

void PreparedStatementCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    statements_.clear();
}

} // namespace ai_backend::core::db