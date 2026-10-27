// 仅用于 A+B+C 联调测试。没有网络调用，不属于 D 实现，不可交付为正式模型服务。
#include "shared/module_api.h"

namespace gateway_contract {
CallResult callModel(const ModelConfig& model, const ChatRequest& request, RequestContext& context) {
    CallResult result;
    result.request_id = context.request_id;
    result.model_id = model.id;
    if (!model.is_mock || model.provider_model.rfind("bc-test-", 0) != 0) {
        result.error = {ErrorCode::InvalidConfig, "BC 测试替身只接受明确标记的模拟配置"};
    } else if (context.cancelled->load()) {
        result.error = {ErrorCode::Cancelled, "BC TEST DOUBLE: cancelled"};
    } else if (RequestContext::Clock::now() >= context.deadline || model.provider_model == "bc-test-timeout") {
        result.error = {ErrorCode::TotalTimeout, "BC TEST DOUBLE: timeout"};
    } else if (model.provider_model == "bc-test-reject-503" || model.provider_model == "bc-test-reject-429") {
        result.http_status = model.provider_model == "bc-test-reject-503" ? 503 : 429;
        result.explicit_rejection = true;
        result.error = {ErrorCode::UpstreamRejected, "BC TEST DOUBLE: explicit rejection"};
    } else if (model.provider_model == "bc-test-network") {
        result.error = {ErrorCode::ConnectionFailed, "BC TEST DOUBLE: connection failed"};
    } else if (model.provider_model == "bc-test-ok") {
        result.success = true;
        result.http_status = 200;
        result.answer = "BC TEST DOUBLE ONLY [" + model.id + "]: " + request.message;
    } else {
        result.error = {ErrorCode::InvalidConfig, "未知 BC 测试场景"};
    }
    return result;
}
}
