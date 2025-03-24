#include "core/db/transaction.h"
#include <spdlog/spdlog.h>

namespace ai_backend::core::db {

Transaction::Transaction()
    : is_active_(false) {
    conn_ = ConnectionPool::GetInstance().GetConnection();
}

Transaction::~Transaction() {
    if (is_active_) {
        try {
            Rollback();
        } catch (const std::exception& e) {
            spdlog::error("Error rolling back transaction in destructor: {}", e.what());
        }
    }
    
    ConnectionPool::GetInstance().ReleaseConnection(conn_);
}

void Transaction::Begin() {
    if (is_active_) {
        throw std::runtime_error("Transaction already active");
    }
    
    txn_ = std::make_unique<pqxx::work>(*conn_);
    is_active_ = true;
}

void Transaction::Commit() {
    if (!is_active_) {
        throw std::runtime_error("No active transaction to commit");
    }
    
    txn_->commit();
    txn_.reset();
    is_active_ = false;
}

void Transaction::Rollback() {
    if (!is_active_) {
        throw std::runtime_error("No active transaction to rollback");
    }
    
    txn_->abort();
    txn_.reset();
    is_active_ = false;
}

pqxx::work& Transaction::GetTxn() {
    if (!is_active_) {
        throw std::runtime_error("No active transaction");
    }
    
    return *txn_;
}

} // namespace ai_backend::core::db