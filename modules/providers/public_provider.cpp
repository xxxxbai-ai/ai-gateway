#include "../../shared/module_api.h"
#include "providers/compatible_chat_adapter.h"

namespace gateway_contract {
namespace {
ErrorCode convert(gateway::ProviderErrorCode code) {
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
} // namespace

CallResult callModel(const ModelConfig& model, const ChatRequest& request, RequestContext& context) {
    CallResult result;
    result.request_id = context.request_id;
    result.model_id = model.id;
    if (!context.cancelled) {
        result.error = {ErrorCode::InvalidRequest, "调用上下文缺少取消状态"};
        return result;
    }
    gateway::ModelConfig actual;
    actual.id = model.id;
    actual.enabled = model.enabled;
    actual.simulated = model.is_mock;
    actual.provider_model = model.provider_model;
    actual.base_url = model.base_url;
    actual.credential_ref = model.credential_ref;
    actual.protocol = model.protocol;
    gateway::ChatRequest input;
    input.question = request.message;
    input.model = request.model.value_or("auto");
    input.stream = false;
    gateway::RequestContext operation;
    operation.request_id = context.request_id;
    operation.deadline = context.deadline;
    operation.connect_timeout = context.connect_timeout;
    operation.first_response_timeout = context.first_response_timeout;
    operation.cancelled = context.cancelled;
    try {
        gateway::CompatibleChatAdapter adapter;
        const auto outcome = adapter.invoke(actual, input, operation);
        result.success = outcome.ok();
        result.http_status = outcome.http_status;
        result.explicit_rejection = outcome.explicit_rejection;
        result.elapsed_ms = outcome.elapsed_ms;
        result.error = {convert(outcome.error_code), outcome.error_message};
        if (result.success) result.answer = outcome.answer;
    } catch (...) {
        result.error = {ErrorCode::InternalError, "模型调用发生内部错误"};
    }
    return result;
}
} // namespace gateway_contract
