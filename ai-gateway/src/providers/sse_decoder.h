#pragma once
#include <functional>
#include <string>
#include <string_view>

namespace gateway {
// 输入是任意网络字节块。只在完整空行分隔事件后调用 handler，不提前解析 UTF-8。
class SseDecoder {
public:
    using Handler = std::function<bool(const std::string& event, const std::string& data)>;
    bool feed(std::string_view bytes, const Handler& handler);
private:
    bool endLine(const Handler& handler);
    std::string line_, event_, data_;
    bool skip_lf_ = false, has_data_ = false;
};
} // namespace gateway
