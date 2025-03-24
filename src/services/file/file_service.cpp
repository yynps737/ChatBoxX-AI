#include "services/file/file_service.h"
#include "core/utils/uuid.h"
#include "core/utils/string_utils.h"
#include "core/config/config_manager.h"
#include "core/db/connection_pool.h"
#include <spdlog/spdlog.h>
#include <pqxx/pqxx>
#include <fstream>
#include <regex>
#include <filesystem>
#include <unordered_set>

namespace ai_backend::services::file {

using namespace core::async;
namespace fs = std::filesystem;

FileService::FileService() {
    auto& config = core::config::ConfigManager::GetInstance();
    
    upload_dir_ = config.GetString("file.upload_dir", "uploads");
    allowed_extensions_ = config.GetStringList("file.allowed_extensions", 
        {".jpg", ".jpeg", ".png", ".gif", ".pdf", ".txt", ".md", ".csv"});
    max_file_size_ = config.GetInt("file.max_size", 10 * 1024 * 1024); // 默认10MB
    base_url_ = config.GetString("file.base_url", "/files");
    
    // 安全措施：确保上传目录不在公共Web目录中
    if (upload_dir_.find("public") != std::string::npos || 
        upload_dir_.find("www") != std::string::npos) {
        spdlog::warn("Upload directory may be in public web space, this poses a security risk");
    }
    
    // 创建上传目录，确保权限正确
    if (!fs::exists(upload_dir_)) {
        fs::create_directories(upload_dir_);
        
        // 尝试设置目录权限（UNIX系统）
        #ifndef _WIN32
        try {
            fs::permissions(upload_dir_, fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec);
        } catch (...) {
            spdlog::warn("Could not set upload directory permissions");
        }
        #endif
    }
    
    // 检查并加载文件类型映射表
    InitMimeTypeMap();
}

void FileService::InitMimeTypeMap() {
    // 添加常见文件类型的映射
    mime_type_map_ = {
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".pdf", "application/pdf"},
        {".txt", "text/plain"},
        {".md", "text/markdown"},
        {".csv", "text/csv"},
        {".doc", "application/msword"},
        {".docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {".xls", "application/vnd.ms-excel"},
        {".xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"}
    };
}

Task<common::Result<models::File>> 
FileService::SaveFile(const models::File& file_info, const std::vector<uint8_t>& data) {
    try {
        // 检查文件大小
        if (data.size() > max_file_size_) {
            co_return common::Result<models::File>::Error("文件大小超过限制");
        }
        
        // 检查文件扩展名
        std::string extension = GetFileExtension(file_info.name);
        if (!IsAllowedExtension(extension)) {
            co_return common::Result<models::File>::Error("不支持的文件类型");
        }
        
        // 验证内容与文件类型匹配
        std::string detected_mime = DetectMimeType(data, extension);
        if (!detected_mime.empty() && file_info.type != detected_mime) {
            spdlog::warn("MIME type mismatch: stated {} but detected {}", 
                      file_info.type, detected_mime);
            
            // 使用检测到的MIME类型
            std::string actual_type = detected_mime;
            
            // 验证检测到的MIME类型是否允许
            bool type_allowed = false;
            for (const auto& ext : allowed_extensions_) {
                if (mime_type_map_.find(ext) != mime_type_map_.end() && 
                    mime_type_map_[ext] == actual_type) {
                    type_allowed = true;
                    break;
                }
            }
            
            if (!type_allowed) {
                co_return common::Result<models::File>::Error("文件内容类型不允许");
            }
        }
        
        // 生成文件ID和存储路径
        std::string file_id = core::utils::UuidGenerator::GenerateUuid();
        std::string safe_filename = SanitizeFilename(file_info.name);
        std::string storage_filename = file_id + extension;
        std::string storage_path = upload_dir_ + "/" + storage_filename;
        
        // 安全检查：确保路径在上传目录内（防止路径遍历）
        fs::path base_path = fs::absolute(upload_dir_);
        fs::path target_path = fs::absolute(storage_path);
        
        if (!IsPathWithinBase(target_path, base_path)) {
            co_return common::Result<models::File>::Error("无效的文件路径");
        }
        
        // 保存文件到磁盘
        std::ofstream file(storage_path, std::ios::binary);
        if (!file) {
            co_return common::Result<models::File>::Error("无法创建文件");
        }
        
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        file.close();
        
        // 生成URL
        std::string url = base_url_ + "/" + storage_filename;
        
        // 保存文件记录到数据库
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        txn.exec_params(
            "INSERT INTO files (id, user_id, message_id, name, type, size, url, created_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, NOW())",
            file_id, file_info.user_id, file_info.message_id, safe_filename, 
            detected_mime.empty() ? file_info.type : detected_mime,
            data.size(), url
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        // 返回完整的文件信息
        models::File saved_file = file_info;
        saved_file.id = file_id;
        saved_file.name = safe_filename;
        saved_file.size = data.size();
        saved_file.url = url;
        saved_file.type = detected_mime.empty() ? file_info.type : detected_mime;
        
        co_return common::Result<models::File>::Ok(saved_file);
    } catch (const std::exception& e) {
        spdlog::error("Error in SaveFile: {}", e.what());
        co_return common::Result<models::File>::Error("保存文件失败");
    }
}

bool FileService::IsPathWithinBase(const fs::path& path, const fs::path& base) {
    // 验证路径是否在基础目录内（防止路径遍历攻击）
    auto rel = fs::relative(path, base);
    auto str = rel.string();
    return !str.empty() && str[0] != '.' && str.find("..") == std::string::npos;
}

std::string FileService::DetectMimeType(const std::vector<uint8_t>& data, const std::string& extension) {
    if (data.size() < 8) {
        return "";
    }
    
    // 通过文件魔数判断文件类型
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        return "image/jpeg";
    } else if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47) {
        return "image/png";
    } else if (data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46) {
        return "image/gif";
    } else if (data[0] == 0x25 && data[1] == 0x50 && data[2] == 0x44 && data[3] == 0x46) {
        return "application/pdf";
    } else if (data[0] == 0x50 && data[1] == 0x4B && data[2] == 0x03 && data[3] == 0x04) {
        // ZIP格式，可能是DOCX, XLSX等
        if (extension == ".docx") {
            return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
        } else if (extension == ".xlsx") {
            return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
        }
    }
    
    // 没有识别出类型，返回扩展名对应的MIME类型
    if (mime_type_map_.find(extension) != mime_type_map_.end()) {
        return mime_type_map_[extension];
    }
    
    return "";
}
}
Task<common::Result<models::File>> 
FileService::GetFileById(const std::string& file_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, user_id, message_id, name, type, size, url, created_at "
            "FROM files WHERE id = $1",
            file_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<models::File>::Error("文件不存在");
        }
        
        models::File file;
        file.id = result[0][0].as<std::string>();
        file.user_id = result[0][1].as<std::string>();
        if (!result[0][2].is_null()) {
            file.message_id = result[0][2].as<std::string>();
        }
        file.name = result[0][3].as<std::string>();
        file.type = result[0][4].as<std::string>();
        file.size = result[0][5].as<size_t>();
        file.url = result[0][6].as<std::string>();
        file.created_at = result[0][7].as<std::string>();
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<models::File>::Ok(file);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFileById: {}", e.what());
        co_return common::Result<models::File>::Error("获取文件失败");
    }
}

Task<common::Result<std::vector<models::File>>> 
FileService::GetFilesByUserId(const std::string& user_id, int page, int page_size) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        
        int offset = (page - 1) * page_size;
        auto result = txn.exec_params(
            "SELECT id, user_id, message_id, name, type, size, url, created_at "
            "FROM files WHERE user_id = $1 "
            "ORDER BY created_at DESC LIMIT $2 OFFSET $3",
            user_id, page_size, offset
        );
        
        std::vector<models::File> files;
        for (const auto& row : result) {
            models::File file;
            file.id = row[0].as<std::string>();
            file.user_id = row[1].as<std::string>();
            if (!row[2].is_null()) {
                file.message_id = row[2].as<std::string>();
            }
            file.name = row[3].as<std::string>();
            file.type = row[4].as<std::string>();
            file.size = row[5].as<size_t>();
            file.url = row[6].as<std::string>();
            file.created_at = row[7].as<std::string>();
            
            files.push_back(file);
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::File>>::Ok(files);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFilesByUserId: {}", e.what());
        co_return common::Result<std::vector<models::File>>::Error("获取文件列表失败");
    }
}

Task<common::Result<std::vector<models::File>>> 
FileService::GetFilesByMessageId(const std::string& message_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, user_id, message_id, name, type, size, url, created_at "
            "FROM files WHERE message_id = $1",
            message_id
        );
        
        std::vector<models::File> files;
        for (const auto& row : result) {
            models::File file;
            file.id = row[0].as<std::string>();
            file.user_id = row[1].as<std::string>();
            file.message_id = row[2].as<std::string>();
            file.name = row[3].as<std::string>();
            file.type = row[4].as<std::string>();
            file.size = row[5].as<size_t>();
            file.url = row[6].as<std::string>();
            file.created_at = row[7].as<std::string>();
            
            files.push_back(file);
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::File>>::Ok(files);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFilesByMessageId: {}", e.what());
        co_return common::Result<std::vector<models::File>>::Error("获取文件列表失败");
    }
}

Task<common::Result<std::vector<uint8_t>>> 
FileService::GetFileContent(const std::string& file_id) {
    try {
        // 先获取文件信息
        auto file_result = co_await GetFileById(file_id);
        if (file_result.IsError()) {
            co_return common::Result<std::vector<uint8_t>>::Error(file_result.GetError());
        }
        
        const auto& file = file_result.GetValue();
        
        // 获取文件路径
        std::string filename = file.url.substr(file.url.find_last_of('/') + 1);
        std::string filepath = upload_dir_ + "/" + filename;
        
        // 读取文件内容
        std::ifstream file_stream(filepath, std::ios::binary);
        if (!file_stream) {
            co_return common::Result<std::vector<uint8_t>>::Error("无法打开文件");
        }
        
        // 获取文件大小
        file_stream.seekg(0, std::ios::end);
        size_t size = file_stream.tellg();
        file_stream.seekg(0, std::ios::beg);
        
        // 读取文件内容到缓冲区
        std::vector<uint8_t> buffer(size);
        file_stream.read(reinterpret_cast<char*>(buffer.data()), size);
        
        co_return common::Result<std::vector<uint8_t>>::Ok(buffer);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFileContent: {}", e.what());
        co_return common::Result<std::vector<uint8_t>>::Error("获取文件内容失败");
    }
}

Task<common::Result<void>> 
FileService::DeleteFile(const std::string& file_id) {
    try {
        // 先获取文件信息
        auto file_result = co_await GetFileById(file_id);
        if (file_result.IsError()) {
            co_return common::Result<void>::Error(file_result.GetError());
        }
        
        const auto& file = file_result.GetValue();
        
        // 获取文件路径
        std::string filename = file.url.substr(file.url.find_last_of('/') + 1);
        std::string filepath = upload_dir_ + "/" + filename;
        
        // 从数据库中删除文件记录
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        txn.exec_params("DELETE FROM files WHERE id = $1", file_id);
        txn.commit();
        
        db_pool.ReleaseConnection(conn);
        
        // 从文件系统中删除文件
        if (fs::exists(filepath)) {
            fs::remove(filepath);
        }
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteFile: {}", e.what());
        co_return common::Result<void>::Error("删除文件失败");
    }
}

Task<common::Result<ParsedFormData>> 
FileService::ParseMultipartFormData(const core::http::Request& request) {
    // 简易实现：返回空解析结果
    ParsedFormData form_data;
    co_return common::Result<ParsedFormData>::Ok(form_data);
}

std::string FileService::GetFileExtension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos);
    }
    return "";
}

bool FileService::IsAllowedExtension(const std::string& extension) {
    for (const auto& allowed_ext : allowed_extensions_) {
        if (core::utils::StringUtils::ToLower(extension) == core::utils::StringUtils::ToLower(allowed_ext)) {
            return true;
        }
    }
    return false;
}

std::string FileService::SanitizeFilename(const std::string& filename) {
    // 移除路径部分，只保留文件名
    std::string sanitized = filename;
    size_t pos = sanitized.find_last_of("/\\");
    if (pos != std::string::npos) {
        sanitized = sanitized.substr(pos + 1);
    }
    
    // 移除危险字符
    sanitized = std::regex_replace(sanitized, std::regex("[^a-zA-Z0-9._-]"), "_");
    
    return sanitized;
}

Task<common::Result<models::File>> 
FileService::GetFileById(const std::string& file_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, user_id, message_id, name, type, size, url, created_at "
            "FROM files WHERE id = $1",
            file_id
        );
        
        if (result.empty()) {
            db_pool.ReleaseConnection(conn);
            co_return common::Result<models::File>::Error("文件不存在");
        }
        
        models::File file;
        file.id = result[0][0].as<std::string>();
        file.user_id = result[0][1].as<std::string>();
        if (!result[0][2].is_null()) {
            file.message_id = result[0][2].as<std::string>();
        }
        file.name = result[0][3].as<std::string>();
        file.type = result[0][4].as<std::string>();
        file.size = result[0][5].as<size_t>();
        file.url = result[0][6].as<std::string>();
        file.created_at = result[0][7].as<std::string>();
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<models::File>::Ok(file);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFileById: {}", e.what());
        co_return common::Result<models::File>::Error("获取文件失败");
    }
}

Task<common::Result<std::vector<models::File>>> 
FileService::GetFilesByUserId(const std::string& user_id, int page, int page_size) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        
        int offset = (page - 1) * page_size;
        auto result = txn.exec_params(
            "SELECT id, user_id, message_id, name, type, size, url, created_at "
            "FROM files WHERE user_id = $1 "
            "ORDER BY created_at DESC LIMIT $2 OFFSET $3",
            user_id, page_size, offset
        );
        
        std::vector<models::File> files;
        for (const auto& row : result) {
            models::File file;
            file.id = row[0].as<std::string>();
            file.user_id = row[1].as<std::string>();
            if (!row[2].is_null()) {
                file.message_id = row[2].as<std::string>();
            }
            file.name = row[3].as<std::string>();
            file.type = row[4].as<std::string>();
            file.size = row[5].as<size_t>();
            file.url = row[6].as<std::string>();
            file.created_at = row[7].as<std::string>();
            
            files.push_back(file);
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::File>>::Ok(files);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFilesByUserId: {}", e.what());
        co_return common::Result<std::vector<models::File>>::Error("获取文件列表失败");
    }
}

Task<common::Result<std::vector<models::File>>> 
FileService::GetFilesByMessageId(const std::string& message_id) {
    try {
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        auto result = txn.exec_params(
            "SELECT id, user_id, message_id, name, type, size, url, created_at "
            "FROM files WHERE message_id = $1",
            message_id
        );
        
        std::vector<models::File> files;
        for (const auto& row : result) {
            models::File file;
            file.id = row[0].as<std::string>();
            file.user_id = row[1].as<std::string>();
            file.message_id = row[2].as<std::string>();
            file.name = row[3].as<std::string>();
            file.type = row[4].as<std::string>();
            file.size = row[5].as<size_t>();
            file.url = row[6].as<std::string>();
            file.created_at = row[7].as<std::string>();
            
            files.push_back(file);
        }
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<std::vector<models::File>>::Ok(files);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFilesByMessageId: {}", e.what());
        co_return common::Result<std::vector<models::File>>::Error("获取文件列表失败");
    }
}

Task<common::Result<std::vector<uint8_t>>> 
FileService::GetFileContent(const std::string& file_id) {
    try {
        // 先获取文件信息
        auto file_result = co_await GetFileById(file_id);
        if (file_result.IsError()) {
            co_return common::Result<std::vector<uint8_t>>::Error(file_result.GetError());
        }
        
        const auto& file = file_result.GetValue();
        
        // 获取文件路径
        std::string filename = file.url.substr(file.url.find_last_of('/') + 1);
        std::string filepath = upload_dir_ + "/" + filename;
        
        // 读取文件内容
        std::ifstream file_stream(filepath, std::ios::binary);
        if (!file_stream) {
            co_return common::Result<std::vector<uint8_t>>::Error("无法打开文件");
        }
        
        // 获取文件大小
        file_stream.seekg(0, std::ios::end);
        size_t size = file_stream.tellg();
        file_stream.seekg(0, std::ios::beg);
        
        // 读取文件内容到缓冲区
        std::vector<uint8_t> buffer(size);
        file_stream.read(reinterpret_cast<char*>(buffer.data()), size);
        
        co_return common::Result<std::vector<uint8_t>>::Ok(buffer);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFileContent: {}", e.what());
        co_return common::Result<std::vector<uint8_t>>::Error("获取文件内容失败");
    }
}

Task<common::Result<void>> 
FileService::DeleteFile(const std::string& file_id) {
    try {
        // 先获取文件信息
        auto file_result = co_await GetFileById(file_id);
        if (file_result.IsError()) {
            co_return common::Result<void>::Error(file_result.GetError());
        }
        
        const auto& file = file_result.GetValue();
        
        // 获取文件路径
        std::string filename = file.url.substr(file.url.find_last_of('/') + 1);
        std::string filepath = upload_dir_ + "/" + filename;
        
        // 从数据库中删除文件记录
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        txn.exec_params("DELETE FROM files WHERE id = $1", file_id);
        txn.commit();
        
        db_pool.ReleaseConnection(conn);
        
        // 从文件系统中删除文件
        if (fs::exists(filepath)) {
            fs::remove(filepath);
        }
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteFile: {}", e.what());
        co_return common::Result<void>::Error("删除文件失败");
    }
}

Task<common::Result<ParsedFormData>> 
FileService::ParseMultipartFormData(const core::http::Request& request) {
    // 简易实现：返回空解析结果
    ParsedFormData form_data;
    co_return common::Result<ParsedFormData>::Ok(form_data);
}

std::string FileService::GetFileExtension(const std::string& filename) {
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos);
    }
    return "";
}

bool FileService::IsAllowedExtension(const std::string& extension) {
    for (const auto& allowed_ext : allowed_extensions_) {
        if (core::utils::StringUtils::ToLower(extension) == core::utils::StringUtils::ToLower(allowed_ext)) {
            return true;
        }
    }
    return false;
}

std::string FileService::SanitizeFilename(const std::string& filename) {
    // 移除路径部分，只保留文件名
    std::string sanitized = filename;
    size_t pos = sanitized.find_last_of("/\\");
    if (pos != std::string::npos) {
        sanitized = sanitized.substr(pos + 1);
    }
    
    // 移除危险字符
    sanitized = std::regex_replace(sanitized, std::regex("[^a-zA-Z0-9._-]"), "_");
    
    return sanitized;
}
