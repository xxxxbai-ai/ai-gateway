// 保留原来的命令行入口与 21 条回归样例；分类业务已提取到 modules/analyzer。
#include "modules/analyzer/question_analyzer.h"
#include "modules/analyzer/cli_support.h"

#include <exception>
#include <iostream>

namespace {
int selfTest() {
    struct Case { const char* question; const char* task; };
    const Case cases[] = {
        {"1+2*3等于多少", "math"},
        {"计算256的平方根", "math"},
        {"解方程 x^2+2x+1=0", "math"},
        {"sqrt(16)等于几", "math"},
        {"100的二进制是多少", "math"},
        {"帮我写一个Python爬虫", "programming"},
        {"C++里面指针和引用有什么区别", "programming"},
        {"这段代码报错了: NullPointerException", "programming"},
        {"for (int i=0;i<n;i++) { }", "programming"},
        {"import numpy as np", "programming"},
        {"用Java实现单例模式", "programming"},
        {"把'你好'翻译成英文", "translation"},
        {"translate this to French", "translation"},
        {"这个单词怎么读", "translation"},
        {"帮我写一篇关于环保的作文", "writing"},
        {"帮我润色一下这段文字", "writing"},
        {"写一个工作总结", "writing"},
        {"什么是人工智能", "general"},
        {"为什么天空是蓝色的", "general"},
        {"介绍一下深度学习", "general"},
        {"今天天气怎么样", "general"},
    };
    int passed = 0;
    for (const auto& item : cases) {
        const auto result = classification::analyzeQuestion(item.question);
        const bool ok = result.task == item.task && !result.uncertain && !result.reason.empty();
        if (ok) ++passed;
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << item.question
                  << " -> " << result.task << '\n';
        if (!ok) analyzer_cli::print(item.question, result);
    }
    const int total = static_cast<int>(sizeof(cases) / sizeof(cases[0]));
    std::cout << "原有样例回归: " << passed << '/' << total << " 通过\n";
    return passed == total ? 0 : 1;
}
} // namespace

int main(int argc, char** argv) {
    try {
        const auto args = analyzer_cli::arguments(argc, argv);
        if (args.size() == 2 && args[1] == "--test") return selfTest();
        if (args.size() == 2 && args[1] == "--help") {
            std::cout << "classifier [问题文本 | --test]\n"
                         "无参数时逐行读取 UTF-8 标准输入。\n";
            return 0;
        }
        if (args.size() > 1) {
            const auto question = analyzer_cli::joinedQuestion(args);
            analyzer_cli::print(question, classification::analyzeQuestion(question));
        } else {
            std::string question;
            while (std::getline(std::cin, question))
                analyzer_cli::print(question, classification::analyzeQuestion(question));
        }
    } catch (const std::exception& error) {
        std::cerr << "错误: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
