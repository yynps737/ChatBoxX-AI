#pragma once

#include <string>

namespace ai_backend::common::error_codes {

struct ErrorCode {
    int code;
    std::string message;
};

// 系统错误（1xxx）
extern const ErrorCode SYSTEM_ERROR;
extern const ErrorCode DATABASE_ERROR;
extern const ErrorCode CONFIG_ERROR;
extern const ErrorCode NETWORK_ERROR;
extern const ErrorCode SERVICE_UNAVAILABLE;
extern const ErrorCode TIMEOUT_ERROR;

// 认证错误（2xxx）
extern const ErrorCode UNAUTHORIZED;
extern const ErrorCode INVALID_CREDENTIALS;
extern const ErrorCode TOKEN_EXPIRED;
extern const ErrorCode INVALID_TOKEN;
extern const ErrorCode FORBIDDEN;
extern const ErrorCode ACCOUNT_LOCKED;
extern const ErrorCode ACCOUNT_DISABLED;

// 请求错误（3xxx）
extern const ErrorCode BAD_REQUEST;
extern const ErrorCode VALIDATION_ERROR;
extern const ErrorCode RESOURCE_NOT_FOUND;
extern const ErrorCode METHOD_NOT_ALLOWED;
extern const ErrorCode DUPLICATE_RESOURCE;
extern const ErrorCode RATE_LIMITED;
extern const ErrorCode REQUEST_TOO_LARGE;

// 对话错误（4xxx）
extern const ErrorCode DIALOG_NOT_FOUND;
extern const ErrorCode DIALOG_CREATION_FAILED;
extern const ErrorCode DIALOG_UPDATE_FAILED;
extern const ErrorCode DIALOG_DELETE_FAILED;
extern const ErrorCode DIALOG_ACCESS_DENIED;

// 消息错误（5xxx）
extern const ErrorCode MESSAGE_NOT_FOUND;
extern const ErrorCode MESSAGE_CREATION_FAILED;
extern const ErrorCode MESSAGE_UPDATE_FAILED;
extern const ErrorCode MESSAGE_DELETE_FAILED;
extern const ErrorCode MESSAGE_TOO_LONG;
extern const ErrorCode MESSAGE_INVALID_FORMAT;

// 文件错误（6xxx）
extern const ErrorCode FILE_NOT_FOUND;
extern const ErrorCode FILE_TOO_LARGE;
extern const ErrorCode FILE_TYPE_NOT_ALLOWED;
extern const ErrorCode FILE_UPLOAD_FAILED;
extern const ErrorCode FILE_DOWNLOAD_FAILED;
extern const ErrorCode FILE_DELETE_FAILED;

// AI模型错误（7xxx）
extern const ErrorCode MODEL_NOT_FOUND;
extern const ErrorCode MODEL_UNAVAILABLE;
extern const ErrorCode MODEL_REQUEST_FAILED;
extern const ErrorCode MODEL_RESPONSE_ERROR;
extern const ErrorCode MODEL_CONTEXT_TOO_LARGE;
extern const ErrorCode MODEL_QUOTA_EXCEEDED;
extern const ErrorCode MODEL_API_ERROR;

// 用户错误（8xxx）
extern const ErrorCode USER_NOT_FOUND;
extern const ErrorCode USER_ALREADY_EXISTS;
extern const ErrorCode USER_CREATION_FAILED;
extern const ErrorCode USER_UPDATE_FAILED;
extern const ErrorCode USER_DELETE_FAILED;
extern const ErrorCode USER_QUOTA_EXCEEDED;

std::string GetErrorMessage(int code);

} // namespace ai_backend::common::error_codes