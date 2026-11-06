#pragma once
#include <array>
#include <string>
#include <vector>

namespace gateway {
enum class TaskType { Coding, Math, Writing, Translation, General };

inline const char* taskName(TaskType type) {
    switch (type) {
    case TaskType::Coding: return "编程";
    case TaskType::Math: return "数学";
    case TaskType::Writing: return "写作";
    case TaskType::Translation: return "翻译";
    case TaskType::General: return "通用问答";
    }
    return "通用问答";
}

// A维护公共结构，修改字段时通知其他成员。
struct Message { std::string role, content; };
struct ChatRequest {
    std::string question;
    std::string model = "auto";
    // 保留 question 兼容原演示；有 messages 时向上游完整传递消息历史。
    std::vector<Message> messages = {};
    bool stream = false;
    int max_tokens = 50000; // 单次生成上限；实际可用值受上游模型限制。
};
struct AnalysisResult {
    TaskType task = TaskType::General;
    std::string reason;
};
struct ModelConfig {
    std::string id;
    // 顺序对应TaskType，演示分数为0～5。
    std::array<int, 5> abilities{};
    bool enabled = true;
    std::string provider_model = {};
    std::string base_url = {}; // API 根地址，如 https://example.com/v1
    std::string credential_ref = {}; // env:环境变量名；不保存秘密值
    std::string protocol = "chat_completions";
    bool simulated = false;
};
struct RouteDecision {
    std::string model_id;
    std::string reason;
};
struct ChatResponse {
    TaskType task;
    std::string model_id;
    std::string answer;
    std::string reason;
};
} // namespace gateway
