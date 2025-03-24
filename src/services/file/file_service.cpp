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
    
    // 创建上传目录
    if (!fs::exists(upload_dir_)) {
        fs::create_directories(upload_dir_);
    }
}

Task<common::Result<models::File>> FileService::GetFileById(const std::string& file_id) {
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
            "FROM files WHERE message_id = $1 "
            "ORDER BY created_at",
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
        
        // 验证文件类型与Content-Type匹配
        bool type_match = false;
        
        // 图片类型验证
        if (extension == ".jpg" || extension == ".jpeg") {
            type_match = (file_info.type == "image/jpeg");
            // 验证JPEG文件头
            if (data.size() >= 2 && data[0] == 0xFF && data[1] == 0xD8) {
                type_match = true;
            }
        } else if (extension == ".png") {
            type_match = (file_info.type == "image/png");
            // 验证PNG文件头
            if (data.size() >= 8 && 
                data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
                data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
                type_match = true;
            }
        } else if (extension == ".gif") {
            type_match = (file_info.type == "image/gif");
            // 验证GIF文件头
            if (data.size() >= 6 && 
                data[0] == 0x47 && data[1] == 0x49 && data[2] == 0x46 && 
                data[3] == 0x38 && (data[4] == 0x39 || data[4] == 0x37) && data[5] == 0x61) {
                type_match = true;
            }
        } else if (extension == ".pdf") {
            type_match = (file_info.type == "application/pdf");
            // 验证PDF文件头
            if (data.size() >= 5 && 
                data[0] == 0x25 && data[1] == 0x50 && data[2] == 0x44 && 
                data[3] == 0x46 && data[4] == 0x2D) {
                type_match = true;
            }
        } else {
            // 其他文件类型，简单验证Content-Type
            type_match = true;
        }
        
        if (!type_match) {
            co_return common::Result<models::File>::Error("文件类型与扩展名不匹配");
        }
        
        // 生成文件ID和存储路径
        std::string file_id = core::utils::UuidGenerator::GenerateUuid();
        std::string safe_filename = SanitizeFilename(file_info.name);
        std::string storage_filename = file_id + extension;
        std::string storage_path = upload_dir_ + "/" + storage_filename;
        
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
            file_id, file_info.user_id, file_info.message_id, safe_filename, file_info.type, 
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
        
        co_return common::Result<models::File>::Ok(saved_file);
    } catch (const std::exception& e) {
        spdlog::error("Error in SaveFile: {}", e.what());
        co_return common::Result<models::File>::Error("保存文件失败");
    }
}

Task<common::Result<std::vector<uint8_t>>> FileService::GetFileContent(const std::string& file_id) {
    try {
        // 获取文件记录
        auto file_result = co_await GetFileById(file_id);
        if (file_result.IsError()) {
            co_return common::Result<std::vector<uint8_t>>::Error(file_result.GetError());
        }
        
        const auto& file = file_result.GetValue();
        
        // 提取存储的文件名
        std::string storage_filename = file.url.substr(file.url.find_last_of('/') + 1);
        std::string storage_path = upload_dir_ + "/" + storage_filename;
        
        // 读取文件内容
        std::ifstream file_stream(storage_path, std::ios::binary);
        if (!file_stream) {
            co_return common::Result<std::vector<uint8_t>>::Error("无法读取文件");
        }
        
        file_stream.seekg(0, std::ios::end);
        size_t file_size = file_stream.tellg();
        file_stream.seekg(0, std::ios::beg);
        
        std::vector<uint8_t> data(file_size);
        file_stream.read(reinterpret_cast<char*>(data.data()), file_size);
        
        co_return common::Result<std::vector<uint8_t>>::Ok(data);
    } catch (const std::exception& e) {
        spdlog::error("Error in GetFileContent: {}", e.what());
        co_return common::Result<std::vector<uint8_t>>::Error("获取文件内容失败");
    }
}

Task<common::Result<void>> FileService::DeleteFile(const std::string& file_id) {
    try {
        // 获取文件记录
        auto file_result = co_await GetFileById(file_id);
        if (file_result.IsError()) {
            co_return common::Result<void>::Error(file_result.GetError());
        }
        
        const auto& file = file_result.GetValue();
        
        // 提取存储的文件名
        std::string storage_filename = file.url.substr(file.url.find_last_of('/') + 1);
        std::string storage_path = upload_dir_ + "/" + storage_filename;
        
        // 删除物理文件
        if (fs::exists(storage_path)) {
            fs::remove(storage_path);
        }
        
        // 删除数据库记录
        auto& db_pool = core::db::ConnectionPool::GetInstance();
        auto conn = co_await db_pool.GetConnectionAsync();
        
        pqxx::work txn(*conn);
        txn.exec_params(
            "DELETE FROM files WHERE id = $1",
            file_id
        );
        
        txn.commit();
        db_pool.ReleaseConnection(conn);
        
        co_return common::Result<void>::Ok();
    } catch (const std::exception& e) {
        spdlog::error("Error in DeleteFile: {}", e.what());
        co_return common::Result<void>::Error("删除文件失败");
    }
}

Task<common::Result<ParsedFormData>> 
FileService::ParseMultipartFormData(const core::http::Request& request) {
    try {
        std::string content_type = request.GetHeader("Content-Type");
        std::string boundary;
        
        // 提取boundary
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos != std::string::npos) {
            boundary = content_type.substr(boundary_pos + 9);
            
            // 移除可能的引号
            if (!boundary.empty() && boundary.front() == '"' && boundary.back() == '"') {
                boundary = boundary.substr(1, boundary.size() - 2);
            }
        } else {
            co_return common::Result<ParsedFormData>::Error("无效的Content-Type，未找到boundary");
        }
        
        // 解析multipart/form-data
        ParsedFormData form_data;
        std::string full_boundary = "--" + boundary;
        std::string end_boundary = full_boundary + "--";
        
        const std::string& body = request.body;
        size_t pos = body.find(full_boundary);
        
        while (pos != std::string::npos) {
            // 跳过boundary
            pos += full_boundary.length();
            
            // 检查是否是结束边界
            if (body.substr(pos, 2) == "--") {
                break;
            }
            
            // 寻找部分的头部和内容
            size_t headers_end = body.find("\r\n\r\n", pos);
            if (headers_end == std::string::npos) {
                break;
            }
            
            // 解析头部
            std::string headers = body.substr(pos, headers_end - pos);
            std::string name;
            std::string filename;
            std::string content_type;
            
            // 查找Content-Disposition
            size_t disp_pos = headers.find("Content-Disposition:");
            if (disp_pos != std::string::npos) {
                size_t name_pos = headers.find("name=\"", disp_pos);
                if (name_pos != std::string::npos) {
                    size_t name_end = headers.find("\"", name_pos + 6);
                    name = headers.substr(name_pos + 6, name_end - name_pos - 6);
                }
                
                size_t filename_pos = headers.find("filename=\"", disp_pos);
                if (filename_pos != std::string::npos) {
                    size_t filename_end = headers.find("\"", filename_pos + 10);
                    filename = headers.substr(filename_pos + 10, filename_end - filename_pos - 10);
                }
            }
            
            // 查找Content-Type
            size_t ct_pos = headers.find("Content-Type:");
            if (ct_pos != std::string::npos) {
                size_t ct_end = headers.find("\r\n", ct_pos);
                content_type = headers.substr(ct_pos + 13, ct_end - ct_pos - 13);
                // 去除前导空格
                content_type.erase(0, content_type.find_first_not_of(" \t"));
            }
            
            // 寻找内容的结尾
            headers_end += 4; // 跳过\r\n\r\n
            
            size_t content_end = body.find(full_boundary, headers_end);
            if (content_end == std::string::npos) {
                break;
            }
            
            // 提取内容（删除末尾的\r\n）
            std::string content = body.substr(headers_end, content_end - headers_end - 2);
            
            // 处理文件或字段
            if (!filename.empty()) {
                // 这是一个文件
                FormFile file;
                file.name = name;
                file.filename = filename;
                file.content_type = content_type;
                file.data = std::vector<uint8_t>(content.begin(), content.end());
                form_data.files.push_back(file);
            } else {
                // 这是一个普通字段
                form_data.fields[name] = content;
            }
            
            // 查找下一个部分
            pos = content_end;
        }
        
        co_return common::Result<ParsedFormData>::Ok(form_data);
    } catch (const std::exception& e) {
        spdlog::error("Error in ParseMultipartFormData: {}", e.what());
        co_return common::Result<ParsedFormData>::Error("解析表单数据失败");
    }
}

std::string FileService::GetFileExtension(const std::string& filename) {
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        return filename.substr(dot_pos);
    }
    return "";
}

bool FileService::IsAllowedExtension(const std::string& extension) {
    std::string lower_ext = core::utils::StringUtils::ToLower(extension);
    for (const auto& allowed : allowed_extensions_) {
        if (lower_ext == core::utils::StringUtils::ToLower(allowed)) {
            return true;
        }
    }
    return false;
}

std::string FileService::SanitizeFilename(const std::string& filename) {
    // 删除路径部分
    std::string base_name = filename;
    size_t slash_pos = base_name.find_last_of("/\\");
    if (slash_pos != std::string::npos) {
        base_name = base_name.substr(slash_pos + 1);
    }
    
    // 替换不安全的字符
    std::regex unsafe_chars("[^a-zA-Z0-9._-]");
    std::string safe_name = std::regex_replace(base_name, unsafe_chars, "_");
    
    return safe_name;
}

} // namespace ai_backend::services::file