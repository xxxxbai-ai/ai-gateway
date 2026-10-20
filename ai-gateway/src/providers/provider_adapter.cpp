#include "providers/provider_adapter.h"
#include <stdexcept>

namespace gateway {
const char* errorName(ProviderErrorCode code) {
    switch (code) {
    case ProviderErrorCode::None: return "none";
    case ProviderErrorCode::InvalidRequest: return "invalid_request";
    case ProviderErrorCode::MissingCredential: return "missing_credential";
    case ProviderErrorCode::ConnectionFailed: return "connection_failed";
    case ProviderErrorCode::UpstreamRejected: return "upstream_rejected";
    case ProviderErrorCode::UpstreamError: return "upstream_error";
    case ProviderErrorCode::InvalidResponse: return "invalid_response";
    case ProviderErrorCode::IncompleteResponse: return "incomplete_response";
    case ProviderErrorCode::ConnectTimeout: return "connect_timeout";
    case ProviderErrorCode::FirstResponseTimeout: return "first_response_timeout";
    case ProviderErrorCode::TotalTimeout: return "total_timeout";
    case ProviderErrorCode::Cancelled: return "cancelled";
    case ProviderErrorCode::CallbackFailed: return "callback_failed";
    }
    return "unknown";
}

std::string ProviderAdapter::invoke(const std::string& model_id, const ChatRequest& request) {
    ModelConfig model;
    model.id = model_id;
    RequestContext context;
    auto result = invoke(model, request, context);
    if (!result.ok()) throw std::runtime_error(result.error_message);
    return result.answer;
}
} // namespace gateway
