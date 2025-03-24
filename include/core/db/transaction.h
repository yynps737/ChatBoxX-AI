#pragma once

#include <memory>
#include <pqxx/pqxx>
#include "connection_pool.h"

namespace ai_backend::core::db {

class Transaction {
public:
    Transaction();
    ~Transaction();
    
    void Begin();
    void Commit();
    void Rollback();
    
    pqxx::work& GetTxn();
    
    template<typename... Args>
    pqxx::result ExecParams(const std::string& sql, Args&&... args) {
        return txn_->exec_params(sql, std::forward<Args>(args)...);
    }
    
    template<typename... Args>
    pqxx::result ExecPrepared(const std::string& name, Args&&... args) {
        return txn_->exec_prepared(name, std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<pqxx::connection> conn_;
    std::unique_ptr<pqxx::work> txn_;
    bool is_active_;
};

} // namespace ai_backend::core::db