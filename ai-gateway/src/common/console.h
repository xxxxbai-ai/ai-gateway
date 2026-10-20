#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace gateway {
inline void enableUtf8Console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

inline std::vector<std::string> utf8Arguments(int argc, char** argv) {
    std::vector<std::string> arguments(argv, argv + argc);
#ifdef _WIN32
    int count = 0;
    auto* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wide) throw std::runtime_error("无法读取命令行");
    arguments.clear();
    for (int i = 0; i < count; ++i) {
        const int length = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string value(static_cast<std::size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, value.data(), length, nullptr, nullptr);
        value.pop_back();
        arguments.push_back(std::move(value));
    }
    LocalFree(wide);
#endif
    return arguments;
}
}
