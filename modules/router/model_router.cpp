#include "model_router.h"

#include <algorithm>
#include <set>

namespace routing {

std::vector<ModelConfig> initialModels() {
    return {
        {"mock_code", "模拟编程模型", true, true,
         {{"programming", 95}, {"math", 70}, {"translation", 55},
          {"writing", 60}, {"general", 70}}},
        {"mock_math", "模拟数学模型", true, true,
         {{"programming", 60}, {"math", 95}, {"translation", 55},
          {"writing", 60}, {"general", 70}}},
        {"mock", "模拟通用模型", true, true,
         {{"programming", 65}, {"math", 65}, {"translation", 90},
          {"writing", 90}, {"general", 90}}},
    };
}

SelectionResult selectModel(const std::string& task,
                            const std::vector<ModelConfig>& models) {
    if (std::find(taskTypes.begin(), taskTypes.end(), task) == taskTypes.end()) {
        return {false, "", "", "未知任务类型：" + task};
    }

    std::set<std::string> ids;
    for (const auto& model : models) {
        if (model.id.empty() || !ids.insert(model.id).second) {
            return {false, "", "", "模型编号不能为空或重复"};
        }
    }

    const ModelConfig* best = nullptr;
    int bestScore = -1;
    for (const auto& model : models) {
        const auto score = model.scores.find(task);
        if (!model.enabled || score == model.scores.end() ||
            score->second < 0 || score->second > 100) {
            continue;
        }
        if (!best || score->second > bestScore ||
            (score->second == bestScore && model.id < best->id)) {
            best = &model;
            bestScore = score->second;
        }
    }
    if (!best) {
        return {false, "", "", "当前任务没有启用且评分有效的模型：" + task};
    }
    return {true, best->id,
            "任务 " + task + " 选择 " + best->id + "，能力评分 " +
                std::to_string(bestScore) +
                " 为可用模型最高分；同分按模型编号字典序选择。",
            ""};
}

}  // namespace routing
