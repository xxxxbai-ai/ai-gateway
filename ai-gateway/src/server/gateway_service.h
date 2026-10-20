#pragma once
#include "analyzer/question_analyzer.h"
#include "providers/provider_adapter.h"
#include "router/router.h"
#include "storage/model_repository.h"
#include <utility>

namespace gateway {
struct GatewayResult {
    AnalysisResult analysis;
    RouteDecision decision; // 保留首选模型与原始理由。
    std::vector<AttemptResult> attempts;
    bool fallback_used = false;
    std::string fallback_reason;
    bool ok() const { return !attempts.empty() && attempts.back().ok(); }
};

// A负责：服务层串联模块，后续HTTP入口只做解析、校验与结果转换。
class GatewayService {
public:
    explicit GatewayService(ProviderAdapter& provider) : provider_(provider) {}
    GatewayService(ProviderAdapter& provider, std::vector<ModelConfig> models)
        : repository_(std::move(models)), provider_(provider) {}
    ChatResponse handle(const ChatRequest& request);
    // A 创建 context 并在工作线程调用；收到首个回调事件前不要提交下行响应头。
    GatewayResult handle(const ChatRequest& request, RequestContext& context, EventSink sink = {});

private:
    QuestionAnalyzer analyzer_;
    Router router_;
    ModelRepository repository_;
    ProviderAdapter& provider_;
};
}
