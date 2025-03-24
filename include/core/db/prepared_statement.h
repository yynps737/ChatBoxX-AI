#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <pqxx/pqxx>

namespace ai_backend::core::db {

class PreparedStatement {
public:
    PreparedStatement(std::shared_ptr<pqxx::connection> conn, const std::string& name, const std::string& sql);
    
    template<typename... Args>
    pqxx::result Execute(Args&&... args) {
        pqxx::work txn(*conn_);
        auto result = txn.exec_prepared(name_, std::forward<Args>(args)...);
        txn.commit();
        return result;
    }
    
    template<typename... Args>
    pqxx::result ExecuteWithTxn(pqxx::work& txn, Args&&... args) {
        return txn.exec_prepared(name_, std::forward<Args>(args)...);
    }
    
    std::string GetName() const;
    std::string GetSQL() const;
    
private:
    std::shared_ptr<pqxx::connection> conn_;
    std::string name_;
    std::string sql_;
};

class PreparedStatementCache {
public:
    PreparedStatementCache(std::shared_ptr<pqxx::connection> conn);
    
    std::shared_ptr<PreparedStatement> GetOrCreate(const std::string& name, const std::string& sql);
    bool HasStatement(const std::string& name) const;
    void Clear();
    
private:
    std::shared_ptr<pqxx::connection> conn_;
    std::unordered_map<std::string, std::shared_ptr<PreparedStatement>> statements_;
    mutable std::mutex mutex_;
};

} // namespace ai_backend::core::db