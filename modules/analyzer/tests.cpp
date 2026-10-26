#include "question_analyzer.h"
#include "cli_support.h"

#include <array>
#include <future>
#include <stdexcept>

namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void concurrencyCheck() {
    // 在本进程首次调用时并发初始化规则，检查多请求不会串用结果。
    std::array<std::future<void>, 8> jobs;
    for (std::size_t i = 0; i < jobs.size(); ++i) {
        jobs[i] = std::async(std::launch::async, [i] {
            const bool coding = i % 2 == 0;
            for (int repeat = 0; repeat < 16; ++repeat) {
                const auto result = classification::analyzeQuestion(
                    coding ? "修复 C++ 代码报错" : "把你好翻译成英文");
                require(result.task == (coding ? "programming" : "translation") &&
                        !result.uncertain && !result.reason.empty(), "并发分类结果错误");
            }
        });
    }
    for (auto& job : jobs) job.get();
}
} // namespace

int main(int argc, char** argv) {
    try {
        analyzer_cli::arguments(argc, argv);
        concurrencyCheck();
        struct Case { const char* question; const char* task; bool uncertain; const char* evidence; };
        const Case cases[] = {
            {"解释 C++ 指针", "programming", false, "关键词“指针”"},
            {"计算256的平方根", "math", false, "关键词“计算”"},
            {"把你好翻译成英文", "translation", false, "翻译意图"},
            {"帮我写一篇环保作文", "writing", false, "写作意图"},
            {"为什么天空是蓝色的", "general", false, "关键词“为什么”"},
            {"", "general", true, "空白"},
            {" \t\r\n\f\v", "general", true, "空白"},
            {"你好", "general", true, "未命中"},
            {"qzxv", "general", true, "未命中"},
            {"C++ 方程", "general", true, "最高分相同"},
            {"请翻译并润色这段文字", "general", true, "同时发现"},
            {"翻译这段 C++ 报错", "translation", false, "翻译意图"},
            {"TRANSLATE this PYTHON comment to French", "translation", false, "翻译意图"},
            {"润色这段 Python 注释", "writing", false, "写作意图"},
            {"帮我写一个函数", "programming", false, "关键词“函数”"},
            {"帮我写个程序", "programming", false, "关键词“程序”"},
            {"请帮我写", "general", true, "未命中"},
            {"写一个工作总结", "writing", false, "关键词“总结”"},
            {"PYTHON 爬虫", "programming", false, "关键词“python”"},
            {"  1+2*3  ", "math", false, "结构规则"},
            {"这个单词怎么读", "translation", false, "关键词“怎么读”"},
            {"翻译这段\nC++ 报错", "translation", false, "翻译意图"},
            {"为什么怎么多少", "general", true, "证据未超过"},
            {"请草拟一段话", "writing", false, "关键词“草拟”"},
        };
        const std::array<std::string, 5> allowed{
            "programming", "math", "translation", "writing", "general"};
        int passed = 0;
        for (const auto& item : cases) {
            const auto actual = classification::analyzeQuestion(item.question);
            require(actual.task == item.task, std::string("任务类型错误: ") + item.question + " -> " + actual.task);
            require(actual.uncertain == item.uncertain, std::string("不确定标志错误: ") + item.question);
            require(!actual.reason.empty() && actual.reason.find(item.evidence) != std::string::npos,
                    std::string("分类理由未说明实际依据: ") + item.question + " -> " + actual.reason);
            bool known = false;
            for (const auto& task : allowed) if (task == actual.task) known = true;
            require(known && (!actual.uncertain || actual.task == "general"), "输出违反公共约定");
            const auto repeated = classification::analyzeQuestion(item.question);
            require(repeated.task == actual.task && repeated.reason == actual.reason &&
                    repeated.uncertain == actual.uncertain, "同一输入的结果不稳定");
            ++passed;
        }
        std::cout << "接口与回退样例: " << passed << "/24 通过\n"
                  << "并发检查: 8 个任务共 128 次调用通过\n";
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
    return 0;
}
