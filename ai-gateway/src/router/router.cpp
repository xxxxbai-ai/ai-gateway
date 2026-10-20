#include "router/router.h"
#include <stdexcept>

namespace gateway {
RouteDecision Router::select(const ChatRequest& request,
                             const AnalysisResult& analysis,
                             const std::vector<ModelConfig>& models) const {
    const ModelConfig* best = nullptr;
    const auto task = static_cast<std::size_t>(analysis.task);
    for (const auto& model : models) {
        if (!model.enabled) continue;
        if (request.model != "auto") {
            if (request.model == model.id) return {model.id, "用户手动指定"};
            continue;
        }
        // 演示策略：选当前任务评分最高的模型；同分按配置顺序。
        if (!best || model.abilities[task] > best->abilities[task]) best = &model;
    }
    if (!best) throw std::runtime_error("没有可用模型，请检查模型ID和启用状态");
    // TODO(C)：增加权限、能力限制、默认回退和候选诊断。
    return {best->id, std::string(taskName(analysis.task)) + "能力评分最高"};
}
}
