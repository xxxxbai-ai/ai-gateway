#include "providers/sse_decoder.h"

namespace gateway {
bool SseDecoder::feed(std::string_view bytes, const Handler& handler) {
    for (char ch : bytes) {
        if (skip_lf_ && ch == '\n') { skip_lf_ = false; continue; }
        skip_lf_ = false;
        if (ch == '\r' || ch == '\n') {
            skip_lf_ = ch == '\r';
            if (!endLine(handler)) return false;
        } else {
            line_ += ch;
        }
    }
    return true;
}

bool SseDecoder::endLine(const Handler& handler) {
    if (line_.empty()) {
        if (!has_data_) { event_.clear(); return true; }
        if (!data_.empty()) data_.pop_back(); // 去掉最后一行 data 加上的换行。
        const bool keep = handler(event_.empty() ? "message" : event_, data_);
        event_.clear(); data_.clear(); has_data_ = false;
        return keep;
    }
    auto line = std::move(line_);
    line_.clear();
    if (line.front() == ':') return true;
    const auto colon = line.find(':');
    const auto field = line.substr(0, colon);
    auto value = colon == std::string::npos ? std::string{} : line.substr(colon + 1);
    if (!value.empty() && value.front() == ' ') value.erase(0, 1);
    if (field == "event") event_ = std::move(value);
    if (field == "data") { data_ += value; data_ += '\n'; has_data_ = true; }
    return true;
}
} // namespace gateway
