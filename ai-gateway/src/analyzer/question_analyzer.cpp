#include "analyzer/question_analyzer.h"
#include <initializer_list>

namespace gateway {
AnalysisResult QuestionAnalyzer::analyze(const ChatRequest& request) const {
    const auto has = [&](std::initializer_list<const char*> words) {
        for (const auto* word : words) {
            if (request.question.find(word) != std::string::npos) return true;
        }
        return false;
    };
    // 仅供跑通框架；B后续加入规则配置、上下文与混合任务判断。
    // 先看动作，避免“翻译Python注释”被当成编程题。
    if (has({"翻译", "translate"}))
        return {TaskType::Translation, "发现翻译意图"};
    if (has({"润色", "写作", "写一篇"}))
        return {TaskType::Writing, "发现写作意图"};
    if (has({"推导", "证明", "方程", "数学"}))
        return {TaskType::Math, "发现数学意图"};
    if (has({"代码", "C++", "Python", "python", "编程"}))
        return {TaskType::Coding, "发现编程线索"};
    return {TaskType::General, "未匹配专门任务，使用通用类别"};
}
}
