#pragma once

#include <string>
#include <vector>
#include <memory>
#include "core/http/request.h"
#include "core/async/task.h"
#include "common/result.h"
#include "models/file.h"

namespace ai_backend::services::file {

struct FormFile {
    std::string name;
    std::string filename;
    std::string content_type;
    std::vector<uint8_t> data;
};

struct ParsedFormData {
    std::unordered_map<std::string, std::string> fields;
    std::vector<FormFile> files;
};

class FileService {
public:
    FileService();
    
    core::async::Task<common::Result<models::File>> GetFileById(const std::string& file_id);
    core::async::Task<common::Result<std::vector<models::File>>> GetFilesByUserId(const std::string& user_id, int page = 1, int page_size = 20);
    core::async::Task<common::Result<std::vector<models::File>>> GetFilesByMessageId(const std::string& message_id);
    
    core::async::Task<common::Result<models::File>> SaveFile(const models::File& file_info, const std::vector<uint8_t>& data);
    core::async::Task<common::Result<std::vector<uint8_t>>> GetFileContent(const std::string& file_id);
    core::async::Task<common::Result<void>> DeleteFile(const std::string& file_id);
    
    core::async::Task<common::Result<ParsedFormData>> ParseMultipartFormData(const core::http::Request& request);

private:
    std::string GetFileExtension(const std::string& filename);
    bool IsAllowedExtension(const std::string& extension);
    std::string SanitizeFilename(const std::string& filename);

private:
    std::string upload_dir_;
    std::vector<std::string> allowed_extensions_;
    size_t max_file_size_;
    std::string base_url_;
};

} // namespace ai_backend::services::file