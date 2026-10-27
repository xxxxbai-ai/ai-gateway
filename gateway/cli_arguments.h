#pragma once
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

namespace app {
// 网关自行处理 Windows Unicode 参数，不依赖 B 的演示工具。
inline std::vector<std::string> arguments(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    int count = 0;
    struct Arguments {
        LPWSTR* data;
        ~Arguments() { if (data) LocalFree(data); }
    } wide{CommandLineToArgvW(GetCommandLineW(), &count)};
    if (!wide.data) throw std::runtime_error("无法读取启动参数");
    std::vector<std::string> result;
    for (int index = 0; index < count; ++index) {
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
            wide.data[index], -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) throw std::runtime_error("启动参数包含无效字符");
        std::string text(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data[index], -1,
            text.data(), size, nullptr, nullptr) != size) throw std::runtime_error("无法转换启动参数");
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
} // namespace app
