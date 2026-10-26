#pragma once

// 仅供演示入口使用；业务接口无需包含此文件。
#include "question_analyzer.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace analyzer_cli {

inline std::vector<std::string> arguments(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    int count = 0;
    struct WideArguments {
        LPWSTR* value;
        ~WideArguments() { if (value) LocalFree(value); }
    } wide{CommandLineToArgvW(GetCommandLineW(), &count)};
    if (!wide.value) throw std::runtime_error("无法读取命令行参数");
    std::vector<std::string> result;
    for (int i = 0; i < count; ++i) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.value[i], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) throw std::runtime_error("命令行参数包含无效字符");
        std::string text(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.value[i], -1,
            text.data(), size, nullptr, nullptr) != size)
            throw std::runtime_error("命令行参数无法转换为 UTF-8");
        text.pop_back();
        result.push_back(std::move(text));
    }
    (void)argc;
    (void)argv;
    return result;
#else
    return {argv, argv + argc};
#endif
}

inline std::string joinedQuestion(const std::vector<std::string>& args) {
    std::string question;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (i > 1) question += ' ';
        question += args[i];
    }
    return question;
}

inline void print(const std::string& question, const classification::AnalysisResult& result) {
    std::cout << "问题: " << question << '\n'
              << "task: " << result.task << '\n'
              << "reason: " << result.reason << '\n'
              << "uncertain: " << (result.uncertain ? "true" : "false") << "\n\n";
}

} // namespace analyzer_cli
