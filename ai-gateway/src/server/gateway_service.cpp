#include "server/gateway_service.h"
#include <algorithm>
#include <stdexcept>

namespace gateway {
ChatResponse GatewayService::handle(const ChatRequest& request) {
    RequestContext context;
    const auto result = handle(request, context);
    if (!result.ok()) throw std::runtime_error(result.attempts.back().error_message);
    const auto& actual = result.attempts.back();
    auto reason = result.decision.reason;
    if (result.fallback_used) reason += "；" + result.fallback_reason;
    return {result.analysis.task, actual.model_id, actual.answer, reason};
}

GatewayResult GatewayService::handle(const ChatRequest& request, RequestContext& context, EventSink sink) {
    auto normalized = request;
    if (!normalized.messages.empty()) {
        // 分类器暂用最后一条用户消息；D 仍收到完整历史，不截断模型输入。
        normalized.question.clear();
        for (auto message = normalized.messages.rbegin(); message != normalized.messages.rend(); ++message) {
            if (message->role == "user") { normalized.question = message->content; break; }
        }
    }
    if (normalized.question.find_first_not_of(" \t\r\n") == std::string::npos)
        throw std::invalid_argument("问题不能为空");
    if (context.request_id.empty()) {
        static std::atomic<unsigned long long> sequence{0};
        const auto now = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        context.request_id = "gateway-" + std::to_string(now) + "-" + std::to_string(++sequence);
    }
    auto candidates = repository_.listModels();
    GatewayResult result;
    result.analysis = analyzer_.analyze(normalized);
    result.decision = router_.select(normalized, result.analysis, candidates);
    auto selected = result.decision.model_id;
    bool output_started = false;
    EventSink forward;
    if (sink) forward = [&](const ProviderEvent& event) {
        output_started = true; // 向调用方发出任何事件后，都不再切换。
        return sink(event);
    };
    for (int attempt = 0; attempt < 2; ++attempt) {
        const auto model = std::find_if(candidates.begin(), candidates.end(),
            [&](const ModelConfig& value) { return value.id == selected; });
        if (model == candidates.end()) throw std::runtime_error("路由结果未对应到模型配置");
        result.attempts.push_back(provider_.invoke(*model, normalized, context, forward));
        const auto& outcome = result.attempts.back();
        const bool can_fallback = attempt == 0 && normalized.model == "auto" && !output_started &&
            !context.cancelled->load() && RequestContext::Clock::now() < context.deadline &&
            outcome.error_code == ProviderErrorCode::UpstreamRejected && outcome.explicit_rejection &&
            (outcome.http_status == 429 || outcome.http_status == 503);
        if (!can_fallback) break;
        // 备用仍交给 C 的路由器选择；D 内部不重试，总 deadline 也不重置。
        model->enabled = false;
        try { selected = router_.select(normalized, result.analysis, candidates).model_id; }
        catch (const std::runtime_error&) { break; }
        result.fallback_used = true;
        result.fallback_reason = "首选模型返回 HTTP " + std::to_string(outcome.http_status) +
            "，尝试备用模型 " + selected;
    }
    // TODO(A)：HTTP 入口接入鉴权、下行断连检测和 Repository 诊断落库。
    return result;
}
}
