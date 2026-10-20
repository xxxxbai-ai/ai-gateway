#pragma once
#include "common/contracts.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>

namespace gateway {
enum class ProviderErrorCode {
    None, InvalidRequest, MissingCredential, ConnectionFailed, UpstreamRejected,
    UpstreamError, InvalidResponse, IncompleteResponse, ConnectTimeout,
    FirstResponseTimeout, TotalTimeout, Cancelled, CallbackFailed
};
const char* errorName(ProviderErrorCode code);

struct Usage {
    // 上游没有提供的字段保持空值；模拟用量由 simulated 单独标记。
    std::optional<long long> input_tokens, output_tokens, total_tokens;
    bool simulated = false;
};
enum class EventKind { Delta, Usage, Finish };
struct ProviderEvent {
    EventKind kind;
    std::string text = {};
    std::optional<Usage> usage = {};
};
using EventSink = std::function<bool(const ProviderEvent&)>;

struct RequestContext {
    using Clock = std::chrono::steady_clock;
    std::string request_id = {};
    // A 创建一次并跨备用调用复用；D 不重新开始总计时。
    Clock::time_point deadline = Clock::now() + std::chrono::seconds(120);
    std::chrono::milliseconds connect_timeout{5000};
    std::chrono::milliseconds first_response_timeout{30000};
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
};

struct AttemptResult {
    std::string request_id, model_id, provider_model, answer;
    ProviderErrorCode error_code = ProviderErrorCode::None;
    std::string error_message;
    long http_status = 0;
    long long elapsed_ms = 0;
    bool explicit_rejection = false;
    bool completed = false;
    std::optional<Usage> usage;
    bool ok() const { return error_code == ProviderErrorCode::None && completed; }
    // 这里只报告上游事实；是否启用备用由 A 决定。
};
} // namespace gateway
