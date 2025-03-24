#include "common/error_codes.h"

namespace ai_backend::common::error_codes {

// 系统错误（1xxx）
const ErrorCode SYSTEM_ERROR              = {1001, "系统内部错误"};
const ErrorCode DATABASE_ERROR            = {1002, "数据库错误"};
const ErrorCode CONFIG_ERROR              = {1003, "配置错误"};
const ErrorCode NETWORK_ERROR             = {1004, "网络错误"};
const ErrorCode SERVICE_UNAVAILABLE       = {1005, "服务不可用"};
const ErrorCode TIMEOUT_ERROR             = {1006, "请求超时"};

// 认证错误（2xxx）
const ErrorCode UNAUTHORIZED              = {2001, "未授权访问"};
const ErrorCode INVALID_CREDENTIALS       = {2002, "用户名或密码错误"};
const ErrorCode TOKEN_EXPIRED             = {2003, "令牌已过期"};
const ErrorCode INVALID_TOKEN             = {2004, "无效的令牌"};
const ErrorCode FORBIDDEN                 = {2005, "禁止访问"};
const ErrorCode ACCOUNT_LOCKED            = {2006, "账号已锁定"};
const ErrorCode ACCOUNT_DISABLED          = {2007, "账号已禁用"};

// 请求错误（3xxx）
const ErrorCode BAD_REQUEST               = {3001, "请求格式错误"};
const ErrorCode VALIDATION_ERROR          = {3002, "参数验证失败"};
const ErrorCode RESOURCE_NOT_FOUND        = {3003, "资源不存在"};
const ErrorCode METHOD_NOT_ALLOWED        = {3004, "方法不允许"};
const ErrorCode DUPLICATE_RESOURCE        = {3005, "资源已存在"};
const ErrorCode RATE_LIMITED              = {3006, "请求频率超限"};
const ErrorCode REQUEST_TOO_LARGE         = {3007, "请求数据过大"};

// 对话错误（4xxx）
const ErrorCode DIALOG_NOT_FOUND          = {4001, "对话不存在"};
const ErrorCode DIALOG_CREATION_FAILED    = {4002, "创建对话失败"};
const ErrorCode DIALOG_UPDATE_FAILED      = {4003, "更新对话失败"};
const ErrorCode DIALOG_DELETE_FAILED      = {4004, "删除对话失败"};
const ErrorCode DIALOG_ACCESS_DENIED      = {4005, "无权访问此对话"};

// 消息错误（5xxx）
const ErrorCode MESSAGE_NOT_FOUND         = {5001, "消息不存在"};
const ErrorCode MESSAGE_CREATION_FAILED   = {5002, "创建消息失败"};
const ErrorCode MESSAGE_UPDATE_FAILED     = {5003, "更新消息失败"};
const ErrorCode MESSAGE_DELETE_FAILED     = {5004, "删除消息失败"};
const ErrorCode MESSAGE_TOO_LONG          = {5005, "消息内容过长"};
const ErrorCode MESSAGE_INVALID_FORMAT    = {5006, "消息格式无效"};

// 文件错误（6xxx）
const ErrorCode FILE_NOT_FOUND            = {6001, "文件不存在"};
const ErrorCode FILE_TOO_LARGE            = {6002, "文件过大"};
const ErrorCode FILE_TYPE_NOT_ALLOWED     = {6003, "文件类型不允许"};
const ErrorCode FILE_UPLOAD_FAILED        = {6004, "文件上传失败"};
const ErrorCode FILE_DOWNLOAD_FAILED      = {6005, "文件下载失败"};
const ErrorCode FILE_DELETE_FAILED        = {6006, "文件删除失败"};

// AI模型错误（7xxx）
const ErrorCode MODEL_NOT_FOUND           = {7001, "模型不存在"};
const ErrorCode MODEL_UNAVAILABLE         = {7002, "模型暂不可用"};
const ErrorCode MODEL_REQUEST_FAILED      = {7003, "模型请求失败"};
const ErrorCode MODEL_RESPONSE_ERROR      = {7004, "模型响应错误"};
const ErrorCode MODEL_CONTEXT_TOO_LARGE   = {7005, "上下文过长"};
const ErrorCode MODEL_QUOTA_EXCEEDED      = {7006, "模型配额已用完"};
const ErrorCode MODEL_API_ERROR           = {7007, "模型API错误"};

// 用户错误（8xxx）
const ErrorCode USER_NOT_FOUND            = {8001, "用户不存在"};
const ErrorCode USER_ALREADY_EXISTS       = {8002, "用户已存在"};
const ErrorCode USER_CREATION_FAILED      = {8003, "创建用户失败"};
const ErrorCode USER_UPDATE_FAILED        = {8004, "更新用户失败"};
const ErrorCode USER_DELETE_FAILED        = {8005, "删除用户失败"};
const ErrorCode USER_QUOTA_EXCEEDED       = {8006, "用户配额已用完"};

std::string GetErrorMessage(int code) {
    // 系统错误
    if (code == SYSTEM_ERROR.code) return SYSTEM_ERROR.message;
    if (code == DATABASE_ERROR.code) return DATABASE_ERROR.message;
    if (code == CONFIG_ERROR.code) return CONFIG_ERROR.message;
    if (code == NETWORK_ERROR.code) return NETWORK_ERROR.message;
    if (code == SERVICE_UNAVAILABLE.code) return SERVICE_UNAVAILABLE.message;
    if (code == TIMEOUT_ERROR.code) return TIMEOUT_ERROR.message;
    
    // 认证错误
    if (code == UNAUTHORIZED.code) return UNAUTHORIZED.message;
    if (code == INVALID_CREDENTIALS.code) return INVALID_CREDENTIALS.message;
    if (code == TOKEN_EXPIRED.code) return TOKEN_EXPIRED.message;
    if (code == INVALID_TOKEN.code) return INVALID_TOKEN.message;
    if (code == FORBIDDEN.code) return FORBIDDEN.message;
    if (code == ACCOUNT_LOCKED.code) return ACCOUNT_LOCKED.message;
    if (code == ACCOUNT_DISABLED.code) return ACCOUNT_DISABLED.message;
    
    // 请求错误
    if (code == BAD_REQUEST.code) return BAD_REQUEST.message;
    if (code == VALIDATION_ERROR.code) return VALIDATION_ERROR.message;
    if (code == RESOURCE_NOT_FOUND.code) return RESOURCE_NOT_FOUND.message;
    if (code == METHOD_NOT_ALLOWED.code) return METHOD_NOT_ALLOWED.message;
    if (code == DUPLICATE_RESOURCE.code) return DUPLICATE_RESOURCE.message;
    if (code == RATE_LIMITED.code) return RATE_LIMITED.message;
    if (code == REQUEST_TOO_LARGE.code) return REQUEST_TOO_LARGE.message;
    
    // 对话错误
    if (code == DIALOG_NOT_FOUND.code) return DIALOG_NOT_FOUND.message;
    if (code == DIALOG_CREATION_FAILED.code) return DIALOG_CREATION_FAILED.message;
    if (code == DIALOG_UPDATE_FAILED.code) return DIALOG_UPDATE_FAILED.message;
    if (code == DIALOG_DELETE_FAILED.code) return DIALOG_DELETE_FAILED.message;
    if (code == DIALOG_ACCESS_DENIED.code) return DIALOG_ACCESS_DENIED.message;
    
    // 消息错误
    if (code == MESSAGE_NOT_FOUND.code) return MESSAGE_NOT_FOUND.message;
    if (code == MESSAGE_CREATION_FAILED.code) return MESSAGE_CREATION_FAILED.message;
    if (code == MESSAGE_UPDATE_FAILED.code) return MESSAGE_UPDATE_FAILED.message;
    if (code == MESSAGE_DELETE_FAILED.code) return MESSAGE_DELETE_FAILED.message;
    if (code == MESSAGE_TOO_LONG.code) return MESSAGE_TOO_LONG.message;
    if (code == MESSAGE_INVALID_FORMAT.code) return MESSAGE_INVALID_FORMAT.message;
    
    // 文件错误
    if (code == FILE_NOT_FOUND.code) return FILE_NOT_FOUND.message;
    if (code == FILE_TOO_LARGE.code) return FILE_TOO_LARGE.message;
    if (code == FILE_TYPE_NOT_ALLOWED.code) return FILE_TYPE_NOT_ALLOWED.message;
    if (code == FILE_UPLOAD_FAILED.code) return FILE_UPLOAD_FAILED.message;
    if (code == FILE_DOWNLOAD_FAILED.code) return FILE_DOWNLOAD_FAILED.message;
    if (code == FILE_DELETE_FAILED.code) return FILE_DELETE_FAILED.message;
    
    // AI模型错误
    if (code == MODEL_NOT_FOUND.code) return MODEL_NOT_FOUND.message;
    if (code == MODEL_UNAVAILABLE.code) return MODEL_UNAVAILABLE.message;
    if (code == MODEL_REQUEST_FAILED.code) return MODEL_REQUEST_FAILED.message;
    if (code == MODEL_RESPONSE_ERROR.code) return MODEL_RESPONSE_ERROR.message;
    if (code == MODEL_CONTEXT_TOO_LARGE.code) return MODEL_CONTEXT_TOO_LARGE.message;
    if (code == MODEL_QUOTA_EXCEEDED.code) return MODEL_QUOTA_EXCEEDED.message;
    if (code == MODEL_API_ERROR.code) return MODEL_API_ERROR.message;
    
    // 用户错误
    if (code == USER_NOT_FOUND.code) return USER_NOT_FOUND.message;
    if (code == USER_ALREADY_EXISTS.code) return USER_ALREADY_EXISTS.message;
    if (code == USER_CREATION_FAILED.code) return USER_CREATION_FAILED.message;
    if (code == USER_UPDATE_FAILED.code) return USER_UPDATE_FAILED.message;
    if (code == USER_DELETE_FAILED.code) return USER_DELETE_FAILED.message;
    if (code == USER_QUOTA_EXCEEDED.code) return USER_QUOTA_EXCEEDED.message;
    
    // 未知错误
    return "未知错误";
}

} // namespace ai_backend::common::error_codes