#include "shared/module_api.h"
#include "providers/compatible_chat_adapter.h"

namespace gateway_contract {
namespace {
ErrorCode publicError(gateway::ProviderErrorCode code) {
    using Old = gateway::ProviderErrorCode;
    switch (code) {
    case Old::None: return ErrorCode::None;
    case Old::InvalidRequest: return ErrorCode::InvalidRequest;
    case Old::MissingCredential: return ErrorCode::MissingCredential;
    case Old::ConnectionFailed: return ErrorCode::ConnectionFailed;
    case Old::UpstreamRejected: return ErrorCode::UpstreamRejected;
    case Old::UpstreamError: return ErrorCode::UpstreamError;
    case Old::InvalidResponse: return ErrorCode::InvalidResponse;
    case Old::IncompleteResponse: return ErrorCode::IncompleteResponse;
    case Old::ConnectTimeout: return ErrorCode::ConnectTimeout;
    case Old::FirstResponseTimeout: return ErrorCode::FirstResponseTimeout;
    case Old::TotalTimeout: return ErrorCode::TotalTimeout;
    case Old::Cancelled: return ErrorCode::Cancelled;
    case Old::CallbackFailed: return ErrorCode::InternalError;
    }
    return ErrorCode::InternalError;
}
}

CallResult callModel(const ModelConfig& model, const ChatRequest& request,
                     RequestContext& context) {
    const auto started = RequestContext::Clock::now();
    CallResult result;
    result.request_id = context.request_id;
    result.model_id = model.id;
    const auto fail = [&](ErrorCode code, const char* message) {
        result.error = {code, message};
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            RequestContext::Clock::now() - started).count();
        return result;
    };
    if (!model.enabled) return fail(ErrorCode::DisabledModel, "指定模型已禁用");
    if (model.id.empty() || model.id == "auto" || model.provider_model.empty() ||
        model.protocol != "chat_completions" ||
        (model.is_mock ? model.base_url.rfind("http://", 0) != 0 &&
                         model.base_url.rfind("https://", 0) != 0
                       : model.base_url.rfind("https://", 0) != 0) ||
        (!model.credential_ref.empty() &&
         (model.credential_ref.rfind("env:", 0) != 0 || model.credential_ref.size() == 4)))
        return fail(ErrorCode::InvalidConfig, "模型连接配置无效");
    if (!model.is_mock && model.credential_ref.empty())
        return fail(ErrorCode::MissingCredential, "真实模型未配置凭证引用");
    if ((request.mode != "auto" && request.mode != "manual") ||
        (request.mode == "auto" && request.model) ||
        (request.mode == "manual" && (!request.model || *request.model != model.id)) ||
        request.message.find_first_not_of(" \t\r\n") == std::string::npos ||
        !context.cancelled || context.connect_timeout.count() <= 0 ||
        context.first_response_timeout.count() <= 0)
        return fail(ErrorCode::InvalidRequest, "请求或调用上下文无效");

    gateway::ModelConfig internal_model;
    internal_model.id = model.id;
    internal_model.enabled = model.enabled;
    internal_model.provider_model = model.provider_model;
    internal_model.base_url = model.base_url;
    internal_model.credential_ref = model.credential_ref;
    internal_model.protocol = model.protocol;
    internal_model.simulated = model.is_mock;
    // 公共 1.0 为单轮完整回答；能力评分由 C 使用，无需转换旧 abilities。
    gateway::ChatRequest internal_request;
    internal_request.question = request.message;
    internal_request.model = model.id;
    internal_request.stream = false;
    gateway::RequestContext internal_context;
    internal_context.request_id = context.request_id;
    internal_context.deadline = context.deadline;
    internal_context.connect_timeout = context.connect_timeout;
    internal_context.first_response_timeout = context.first_response_timeout;
    internal_context.cancelled = context.cancelled;

    gateway::CompatibleChatAdapter adapter;
    const auto attempt = adapter.invoke(internal_model, internal_request, internal_context);
    result.success = attempt.ok();
    result.answer = attempt.answer;
    result.http_status = attempt.http_status;
    result.explicit_rejection = attempt.explicit_rejection;
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        RequestContext::Clock::now() - started).count();
    result.error = {publicError(attempt.error_code), attempt.error_message};
    return result;
}
} // namespace gateway_contract
