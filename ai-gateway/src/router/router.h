#pragma once
#include "common/contracts.h"
#include <vector>

namespace gateway {
// C负责：从配置中选择模型，不发起网络调用。
class Router {
public:
    RouteDecision select(const ChatRequest& request,
                         const AnalysisResult& analysis,
                         const std::vector<ModelConfig>& models) const;
};
}
