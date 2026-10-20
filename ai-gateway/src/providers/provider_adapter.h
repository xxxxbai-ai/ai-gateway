#pragma once
#include "providers/provider_types.h"

namespace gateway {
// D负责：统一模型调用接口。适配器只调用一次，不自行选模或重试。
class ProviderAdapter {
public:
    virtual ~ProviderAdapter() = default;
    virtual AttemptResult invoke(const ModelConfig& model,
                                 const ChatRequest& request,
                                 RequestContext& context,
                                 EventSink sink = {}) = 0;
    // 兼容已有 A 模块和本地 MockProvider；网络调用使用上面的完整配置接口。
    std::string invoke(const std::string& model_id, const ChatRequest& request);
};
}
