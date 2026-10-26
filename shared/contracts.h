#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 公共接口 1.0；版本号与项目 V0.1/V0.2 里程碑分别管理。
// 本文件由 A 维护；B/C/D 的内部实现通过适配层使用这些类型。
namespace gateway_contract {

inline constexpr std::string_view version = "1.0";
inline constexpr std::array<std::string_view, 5> task_names{
    "programming", "math", "translation", "writing", "general"};

inline bool isTaskName(std::string_view task) {
    for (const auto value : task_names) if (value == task) return true;
    return false;
}

struct ChatRequest {
    std::string mode = "auto";           // 仅 auto/manual
    std::optional<std::string> model;     // auto 为 nullopt；manual 为非空模型编号
    std::string message;                 // UTF-8，A 校验并去除首尾 ASCII 空白
};

struct AnalysisResult {
    std::string task = "general";
    std::string reason;
    bool uncertain = true;               // true 时 task 必须为 general
};

struct ModelConfig {
    std::string id;
    std::string name;
    bool enabled = true;
    bool is_mock = true;
    std::map<std::string, int> scores;    // 按任务名索引，人工预设整数 0–100
    std::string provider_model;          // 上游实际模型名称，可与 id 不同
    std::string base_url;                // API 根地址，例如 http://127.0.0.1:18080/v1
    std::string credential_ref;          // env:变量名；仅引用，不保存密钥值
    std::string protocol = "chat_completions";
};

enum class ErrorCode {
    None,
    InvalidRequest,
    UnknownModel,
    DisabledModel,
    InvalidConfig,
    NoAvailableModel,
    MissingCredential,
    ConnectionFailed,
    UpstreamRejected,
    UpstreamError,
    InvalidResponse,
    IncompleteResponse,
    ConnectTimeout,
    FirstResponseTimeout,
    TotalTimeout,
    Cancelled,
    InternalError
};

struct Error {
    ErrorCode code = ErrorCode::None;
    std::string message;                 // UTF-8 可读信息，不能包含密钥
};

inline const char* errorCodeName(ErrorCode code) {
    switch (code) {
    case ErrorCode::None: return "none";
    case ErrorCode::InvalidRequest: return "invalid_request";
    case ErrorCode::UnknownModel: return "unknown_model";
    case ErrorCode::DisabledModel: return "disabled_model";
    case ErrorCode::InvalidConfig: return "invalid_config";
    case ErrorCode::NoAvailableModel: return "no_available_model";
    case ErrorCode::MissingCredential: return "missing_credential";
    case ErrorCode::ConnectionFailed: return "connection_failed";
    case ErrorCode::UpstreamRejected: return "upstream_rejected";
    case ErrorCode::UpstreamError: return "upstream_error";
    case ErrorCode::InvalidResponse: return "invalid_response";
    case ErrorCode::IncompleteResponse: return "incomplete_response";
    case ErrorCode::ConnectTimeout: return "connect_timeout";
    case ErrorCode::FirstResponseTimeout: return "first_response_timeout";
    case ErrorCode::TotalTimeout: return "total_timeout";
    case ErrorCode::Cancelled: return "cancelled";
    case ErrorCode::InternalError: return "internal_error";
    }
    return "internal_error";
}

inline int httpStatus(ErrorCode code) {
    switch (code) {
    case ErrorCode::None: return 200;
    case ErrorCode::InvalidRequest:
    case ErrorCode::UnknownModel:
    case ErrorCode::DisabledModel: return 400;
    case ErrorCode::Cancelled: return 408;
    case ErrorCode::InvalidConfig:
    case ErrorCode::InternalError: return 500;
    case ErrorCode::InvalidResponse:
    case ErrorCode::IncompleteResponse: return 502;
    case ErrorCode::ConnectTimeout:
    case ErrorCode::FirstResponseTimeout:
    case ErrorCode::TotalTimeout: return 504;
    default: return 503;
    }
}

struct ModelListResult {
    bool success = false;
    std::vector<ModelConfig> models;
    Error error;
};

struct OperationResult {
    bool success = false;
    Error error;
};

struct RouteResult {
    bool success = false;
    std::string primary_model;           // 失败时为空
    std::optional<std::string> fallback_model;
    std::string reason;
    Error error;
};

struct RequestContext {
    using Clock = std::chrono::steady_clock;
    std::string request_id;              // A 创建，全部尝试共用
    Clock::time_point deadline = Clock::now() + std::chrono::seconds(120);
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds first_response_timeout{30000};
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
};

struct CallResult {
    bool success = false;                // 只有完整回答成功时才为 true
    std::string request_id;
    std::string model_id;                // 实际尝试的公共 id；失败也要填写
    std::string answer;                  // 失败时不作为成功回答展示
    std::int64_t elapsed_ms = 0;
    long http_status = 0;                // 上游 HTTP 状态；无 HTTP 响应时为 0
    bool explicit_rejection = false;     // D 报告事实，A 决定是否切换
    Error error;
};

// A 只对自动模式的首次、明确 429/503 拒绝考虑备用；总调用次数仍由 A 限为两次。
inline bool mayUseFallback(const ChatRequest& request, const CallResult& first,
                           const RequestContext& context, int attempts_made,
                           bool output_started = false) {
    return request.mode == "auto" && !request.model && attempts_made == 1 &&
        !first.success && first.error.code == ErrorCode::UpstreamRejected &&
        first.explicit_rejection && (first.http_status == 429 || first.http_status == 503) &&
        !output_started && context.cancelled && !context.cancelled->load() &&
        RequestContext::Clock::now() < context.deadline;
}

} // namespace gateway_contract
