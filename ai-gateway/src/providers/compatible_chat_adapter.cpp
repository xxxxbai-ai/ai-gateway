#include "providers/compatible_chat_adapter.h"
#include "providers/credential_resolver.h"
#include "providers/sse_decoder.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <memory>
#include <stdexcept>

namespace gateway {
namespace {
using Json = nlohmann::json;
using Clock = RequestContext::Clock;

struct CurlRuntime {
    CURLcode status = curl_global_init(CURL_GLOBAL_DEFAULT);
    ~CurlRuntime() { if (status == CURLE_OK) curl_global_cleanup(); }
};

std::optional<Usage> readUsage(const Json& object, bool simulated) {
    if (!object.contains("usage") || object["usage"].is_null()) return {};
    const auto& value = object.at("usage");
    if (!value.is_object()) throw std::runtime_error("usage 格式错误");
    Usage usage;
    usage.simulated = simulated;
    const auto read = [&](const char* key) -> std::optional<long long> {
        if (!value.contains(key) || value[key].is_null()) return {};
        if (!value[key].is_number_integer()) throw std::runtime_error("用量必须是整数");
        const auto count = value[key].get<long long>();
        if (count < 0) throw std::runtime_error("用量不能为负数");
        return count;
    };
    usage.input_tokens = read("prompt_tokens");
    usage.output_tokens = read("completion_tokens");
    usage.total_tokens = read("total_tokens");
    if (!usage.input_tokens && !usage.output_tokens && !usage.total_tokens) return {};
    return usage;
}

struct Transfer {
    RequestContext& context;
    AttemptResult& result;
    EventSink& sink;
    bool stream, simulated;
    Clock::time_point started = Clock::now();
    bool received_answer = false, saw_done = false;
    std::string body = {};
    SseDecoder decoder{};

    void fail(ProviderErrorCode code, const std::string& message) {
        if (result.error_code == ProviderErrorCode::None) {
            result.error_code = code;
            result.error_message = message;
        }
    }

    bool stopped() {
        if (result.error_code != ProviderErrorCode::None) return true;
        if (context.cancelled->load()) fail(ProviderErrorCode::Cancelled, "请求已取消");
        else if (Clock::now() >= context.deadline)
            fail(ProviderErrorCode::TotalTimeout, "请求总时限已到");
        else if (!received_answer && Clock::now() - started >= context.first_response_timeout)
            fail(ProviderErrorCode::FirstResponseTimeout, "等待首个有效回答超时");
        return result.error_code != ProviderErrorCode::None;
    }

    bool emit(const ProviderEvent& event) {
        if (stopped()) return false;
        if (!sink) return true;
        try {
            if (sink(event)) return true;
            context.cancelled->store(true);
            fail(ProviderErrorCode::Cancelled, "调用方停止接收");
        } catch (...) {
            fail(ProviderErrorCode::CallbackFailed, "调用方事件处理失败");
        }
        return false;
    }

    bool delta(const std::string& text) {
        if (text.empty()) return true; // role 或空 delta 不算有效首段。
        received_answer = true;
        result.answer += text;
        return emit({EventKind::Delta, text});
    }

    bool usage(const Json& value) {
        if (auto report = readUsage(value, simulated)) {
            result.usage = report;
            return emit({EventKind::Usage, {}, report});
        }
        return true;
    }

    bool event(const std::string& type, const std::string& data) {
        if (saw_done) return true;
        if (type == "error") {
            fail(ProviderErrorCode::UpstreamError, "上游返回流内错误");
            return false;
        }
        if (data.empty()) return true;
        if (data == "[DONE]") {
            saw_done = true;
            received_answer = true; // 允许合法的空回答结束事件。
            return true;
        }
        const auto value = Json::parse(data);
        if (value.contains("error")) {
            fail(ProviderErrorCode::UpstreamError, "上游返回流内错误");
            return false;
        }
        if (value.contains("model") && value["model"].is_string())
            result.provider_model = value["model"].get<std::string>();
        if (!value.contains("choices") || !value["choices"].is_array())
            throw std::runtime_error("响应缺少 choices");
        if (!value["choices"].empty()) {
            const auto& choice = value["choices"].at(0);
            const auto& part = choice.at("delta");
            if (part.contains("content") && !part["content"].is_null()) {
                if (!delta(part["content"].get<std::string>())) return false;
            }
        }
        return usage(value);
    }

    bool consume(const char* data, std::size_t length) {
        if (stopped()) return false;
        // 非成功 HTTP 响应不作为回答，也不把供应商原始错误/凭证写入日志。
        if (result.http_status < 200 || result.http_status >= 300) return true;
        if (!stream) { body.append(data, length); return true; }
        return decoder.feed({data, length}, [&](const auto& type, const auto& value) {
            return event(type, value);
        });
    }
};

std::size_t receiveBody(char* data, std::size_t size, std::size_t count, void* user) {
    auto& transfer = *static_cast<Transfer*>(user);
    try {
        return transfer.consume(data, size * count) ? size * count : 0;
    } catch (...) {
        transfer.fail(ProviderErrorCode::InvalidResponse, "上游响应不是有效的文本聊天数据");
        return 0;
    }
}

std::size_t receiveHeader(char* data, std::size_t size, std::size_t count, void* user) {
    auto& transfer = *static_cast<Transfer*>(user);
    try {
        const std::string line(data, size * count);
        long status = 0;
        if (std::sscanf(line.c_str(), "HTTP/%*s %ld", &status) == 1)
            transfer.result.http_status = status;
        return size * count;
    } catch (...) {
        transfer.fail(ProviderErrorCode::InvalidResponse, "上游响应头解析失败");
        return 0;
    }
}

int progress(void* user, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<Transfer*>(user)->stopped() ? 1 : 0;
}

bool validRequest(const ModelConfig& model, const ChatRequest& request) {
    if (model.id.empty() || !model.enabled || model.protocol != "chat_completions" ||
        request.max_tokens <= 0) return false;
    if (model.base_url.rfind("http://", 0) != 0 && model.base_url.rfind("https://", 0) != 0)
        return false;
    if (request.messages.empty())
        return request.question.find_first_not_of(" \t\r\n") != std::string::npos;
    bool has_user = false;
    for (const auto& message : request.messages) {
        if (message.role != "system" && message.role != "user" && message.role != "assistant")
            return false;
        if (message.role == "user" && message.content.find_first_not_of(" \t\r\n") != std::string::npos)
            has_user = true;
    }
    return has_user;
}
} // namespace

AttemptResult CompatibleChatAdapter::invoke(const ModelConfig& model, const ChatRequest& request,
                                            RequestContext& context, EventSink sink) {
    const auto started = Clock::now();
    AttemptResult result;
    result.request_id = context.request_id;
    result.model_id = model.id;
    result.provider_model = model.provider_model.empty() ? model.id : model.provider_model;
    Transfer transfer{context, result, sink, request.stream, model.simulated};
    const auto finish = [&]() {
        result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
        return result;
    };
    if (transfer.stopped()) return finish();
    if (!validRequest(model, request) || context.connect_timeout.count() <= 0 || context.first_response_timeout.count() <= 0) {
        transfer.fail(ProviderErrorCode::InvalidRequest, "检查模型地址、启停状态、消息、协议和调用限制");
        return finish();
    }
    std::string secret;
    try { secret = CredentialResolver::resolve(model.credential_ref); }
    catch (const std::exception& error) {
        transfer.fail(ProviderErrorCode::MissingCredential, error.what());
        return finish();
    }
    static CurlRuntime runtime;
    if (runtime.status != CURLE_OK) {
        transfer.fail(ProviderErrorCode::ConnectionFailed, "libcurl 初始化失败");
        return finish();
    }

    std::string body;
    try {
        Json messages = Json::array();
        if (request.messages.empty()) messages.push_back({{"role", "user"}, {"content", request.question}});
        else for (const auto& message : request.messages)
            messages.push_back({{"role", message.role}, {"content", message.content}});
        // 不额外发送供应商未承诺支持的参数，usage 在上游提供时读取。
        body = Json{{"model", result.provider_model}, {"messages", messages},
                    {"stream", request.stream}, {"max_tokens", request.max_tokens}}.dump();
    } catch (const Json::exception&) {
        transfer.fail(ProviderErrorCode::InvalidRequest, "模型名和消息必须是有效的 UTF-8 文本");
        return finish();
    }
    auto url = model.base_url;
    while (!url.empty() && url.back() == '/') url.pop_back();
    url += "/chat/completions";

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy(curl_easy_init(), curl_easy_cleanup);
    std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)> multi(curl_multi_init(), curl_multi_cleanup);
    if (!easy || !multi) {
        transfer.fail(ProviderErrorCode::ConnectionFailed, "无法创建 HTTP 连接");
        return finish();
    }
    curl_slist* raw_headers = nullptr;
    raw_headers = curl_slist_append(raw_headers, "Content-Type: application/json");
    raw_headers = curl_slist_append(raw_headers, request.stream ? "Accept: text/event-stream" : "Accept: application/json");
    raw_headers = curl_slist_append(raw_headers, "Expect:");
    if (!secret.empty()) raw_headers = curl_slist_append(raw_headers, ("Authorization: Bearer " + secret).c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(raw_headers, curl_slist_free_all);
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(context.deadline - Clock::now()).count();
    if (transfer.stopped()) return finish();
    auto* handle = easy.get();
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::min<long long>(context.connect_timeout.count(), LONG_MAX)));
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, static_cast<long>(std::clamp<long long>(remaining, 1, LONG_MAX)));
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, receiveBody);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, receiveHeader);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, &transfer);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 0L);
    // localhost 模拟调用不经过系统代理；真实 HTTPS 保持 libcurl 默认代理行为。
    curl_easy_setopt(handle, CURLOPT_NOPROXY, "localhost,127.0.0.1,::1");
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
#endif

    CURLcode transport_code = CURLE_OK;
    if (curl_multi_add_handle(multi.get(), handle) != CURLM_OK) {
        transfer.fail(ProviderErrorCode::ConnectionFailed, "无法启动 HTTP 连接");
        return finish();
    }
    int running = 0;
    do {
        if (transfer.stopped()) break;
        if (curl_multi_perform(multi.get(), &running) != CURLM_OK) {
            transfer.fail(ProviderErrorCode::ConnectionFailed, "HTTP 传输失败");
            break;
        }
        int messages_left = 0;
        while (auto* message = curl_multi_info_read(multi.get(), &messages_left))
            if (message->msg == CURLMSG_DONE) transport_code = message->data.result;
        if (!running || transfer.saw_done || transfer.stopped()) break;
        // 每 50ms 检查取消/截止时间；不会阻塞 A 的下行线程。
        if (curl_multi_poll(multi.get(), nullptr, 0, 50, nullptr) != CURLM_OK) {
            transfer.fail(ProviderErrorCode::ConnectionFailed, "HTTP 等待失败");
            break;
        }
    } while (running);
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &result.http_status);
    curl_multi_remove_handle(multi.get(), handle); // 取消或 [DONE] 后立即关闭本次传输。

    if (result.error_code != ProviderErrorCode::None) return finish();
    if (transport_code != CURLE_OK && !transfer.saw_done) {
        if (transfer.stopped()) return finish();
        if (transport_code == CURLE_OPERATION_TIMEDOUT) {
            curl_off_t connected_at = 0;
            curl_easy_getinfo(handle, CURLINFO_CONNECT_TIME_T, &connected_at);
            transfer.fail(connected_at == 0 ? ProviderErrorCode::ConnectTimeout : ProviderErrorCode::TotalTimeout,
                          connected_at == 0 ? "连接上游超时" : "请求总时限已到");
        } else if (request.stream && transfer.received_answer) {
            transfer.fail(ProviderErrorCode::IncompleteResponse, "上游流中断，未收到正常完成标记");
        } else {
            transfer.fail(ProviderErrorCode::ConnectionFailed, "连接上游失败或传输中断");
        }
        return finish();
    }
    if (result.http_status < 200 || result.http_status >= 300) {
        result.explicit_rejection = result.http_status == 429 || result.http_status == 503;
        transfer.fail(result.explicit_rejection ? ProviderErrorCode::UpstreamRejected : ProviderErrorCode::UpstreamError,
                      "上游返回 HTTP " + std::to_string(result.http_status));
        return finish();
    }
    if (request.stream) {
        if (!transfer.saw_done) {
            transfer.fail(ProviderErrorCode::IncompleteResponse, "上游流缺少 [DONE] 完成标记");
            return finish();
        }
    } else {
        try {
            const auto value = Json::parse(transfer.body);
            if (value.contains("error")) {
                transfer.fail(ProviderErrorCode::UpstreamError, "上游返回错误响应");
                return finish();
            }
            const auto& choice = value.at("choices").at(0);
            if (!choice.contains("finish_reason") || !choice["finish_reason"].is_string())
                throw std::runtime_error("响应缺少完成原因");
            if (choice["finish_reason"] != "stop") {
                transfer.fail(ProviderErrorCode::IncompleteResponse, "上游未正常完成文本回答");
                return finish();
            }
            transfer.received_answer = true;
            if (value.contains("model") && value["model"].is_string())
                result.provider_model = value["model"].get<std::string>();
            if (!transfer.delta(choice.at("message").at("content").get<std::string>()) || !transfer.usage(value))
                return finish();
        } catch (...) {
            transfer.fail(ProviderErrorCode::InvalidResponse, "上游响应不是完整的文本聊天 JSON");
            return finish();
        }
    }
    if (transfer.emit({EventKind::Finish})) result.completed = true;
    return finish();
}
} // namespace gateway
