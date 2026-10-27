#include "question_analyzer.h"
#include "cli_support.h"

#include <exception>

int main(int argc, char** argv) {
    try {
        const auto args = analyzer_cli::arguments(argc, argv);
        if (args.size() > 1) {
            const auto question = analyzer_cli::joinedQuestion(args);
            analyzer_cli::print(question, classification::analyzeQuestion(question));
        } else {
            const char* examples[] = {
                "解释这段 C++ 代码报错",
                "计算256的平方根",
                "把你好翻译成英文",
                "帮我写一篇关于环保的作文",
                "为什么天空是蓝色的",
            };
            for (const auto* question : examples)
                analyzer_cli::print(question, classification::analyzeQuestion(question));
        }
    } catch (const std::exception& error) {
        std::cerr << "错误: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
