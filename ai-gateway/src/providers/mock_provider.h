#pragma once
#include "providers/provider_adapter.h"

namespace gateway {
class MockProvider final : public ProviderAdapter {
public:
    using ProviderAdapter::invoke;
    AttemptResult invoke(const ModelConfig& model, const ChatRequest& request,
                         RequestContext& context, EventSink sink = {}) override;
};
}
