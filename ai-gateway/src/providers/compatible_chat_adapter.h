#pragma once
#include "providers/provider_adapter.h"

namespace gateway {
// 文本 chat/completions 子集；支持 HTTP 模拟服务及 HTTPS 真实服务。
class CompatibleChatAdapter final : public ProviderAdapter {
public:
    using ProviderAdapter::invoke;
    AttemptResult invoke(const ModelConfig& model, const ChatRequest& request,
                         RequestContext& context, EventSink sink = {}) override;
};
}
