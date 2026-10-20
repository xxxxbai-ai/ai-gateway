#pragma once

#include <map>
#include <string>
#include <vector>

namespace routing {

inline const std::vector<std::string> taskTypes{
    "programming", "math", "translation", "writing", "general"};

// C 模块内部草案，接入前由 A/C/D 共同确认。
struct ModelConfig {
    std::string id;
    std::string name;
    bool enabled = true;
    bool is_mock = true;
    std::map<std::string, int> scores;
};

struct SelectionResult {
    bool success = false;
    std::string selected_model;
    std::string reason;
    std::string error;
};

std::vector<ModelConfig> initialModels();
SelectionResult selectModel(const std::string& task,
                            const std::vector<ModelConfig>& models);

}  // namespace routing
