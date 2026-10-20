#pragma once
#include "common/contracts.h"

namespace gateway {
// B负责：只分析问题，不访问模型或数据库。
class QuestionAnalyzer {
public:
    AnalysisResult analyze(const ChatRequest& request) const;
};
}
