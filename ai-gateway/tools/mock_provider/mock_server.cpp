#include "mock_server.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>

namespace gateway {
namespace {
using Json = nlohmann::json;
using namespace std::chrono_literals;
const std::vector<std::string> models{"demo-code", "demo-language", "demo-general"};
const Json mock_usage{{"prompt_tokens", 12}, {"completion_tokens", 20}, {"total_tokens", 32}};
std::string event(const Json& value) { return "data: " + value.dump() + "\r\n\r\n"; }
Json chunk(const std::string& model, const std::string& text) {
    return {{"model", model}, {"choices", Json::array({{{"index", 0},
        {"delta", {{"content", text}}}, {"finish_reason", nullptr}}})}};
}
void jsonReply(httplib::Response& response, const Json& value, int status = 200) {
    response.status = status;
    response.set_content(value.dump(), "application/json; charset=utf-8");
}
}

struct MockHttpServer::Impl {
    httplib::Server server;
    int segment_delay;
    std::mutex mutex;
    std::map<std::string, int> counts{{"demo-code", 0}, {"demo-language", 0}, {"demo-general", 0}};
    std::atomic_int active_streams{0}, interrupted_streams{0};
    int last_message_count = 0, last_max_tokens = 0;

    explicit Impl(int delay) : segment_delay(delay) {
        server.new_task_queue = [] { return new httplib::ThreadPool(4); };
        server.set_write_timeout(2s);
        server.Get("/health", [](const auto&, auto& response) {
            jsonReply(response, {{"status", "ok"}, {"simulated", true}});
        });
        server.Get("/v1/models", [](const auto&, auto& response) {
            Json list = Json::array();
            for (const auto& model : models) list.push_back({{"id", model}, {"simulated", true}});
            jsonReply(response, {{"data", list}});
        });
        server.Get("/stats", [this](const auto&, auto& response) {
            std::lock_guard<std::mutex> lock(mutex);
            // 不记录问题原文或 Authorization，只保留次数与字段数量。
            jsonReply(response, {{"counts", counts}, {"active_streams", active_streams.load()},
                {"interrupted_streams", interrupted_streams.load()},
                {"last_message_count", last_message_count}, {"last_max_tokens", last_max_tokens}});
        });
        server.Post(R"(/(?:scenario/([a-z0-9_]+)/)?v1/chat/completions)",
            [this](const httplib::Request& request, httplib::Response& response) { handle(request, response); });
    }

    void handle(const httplib::Request& request, httplib::Response& response) {
        std::string model;
        bool stream = false;
        Json input;
        try {
            input = Json::parse(request.body);
            model = input.at("model").get<std::string>();
            stream = input.value("stream", false);
            if (!input.at("messages").is_array() || input["messages"].empty() || input.value("max_tokens", 512) <= 0)
                throw std::runtime_error("invalid messages");
        } catch (...) {
            jsonReply(response, {{"error", {{"message", "请求格式错误"}}}}, 400);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!counts.count(model)) {
                jsonReply(response, {{"error", {{"message", "模拟模型不存在"}}}}, 404);
                return;
            }
            ++counts[model];
            last_message_count = static_cast<int>(input["messages"].size());
            last_max_tokens = input.value("max_tokens", 512);
        }
        const std::string scenario = request.matches[1].str().empty() ? "normal" : request.matches[1].str();
        if (scenario == "reject429" || scenario == "reject503" || scenario == "unauthorized") {
            const int status = scenario == "reject429" ? 429 : scenario == "reject503" ? 503 : 401;
            jsonReply(response, {{"error", {{"message", "模拟上游明确拒绝"}}}}, status);
            return;
        }
        if (scenario == "delay") {
            for (int i = 0; i < 200; ++i) {
                if (request.is_connection_closed()) return;
                std::this_thread::sleep_for(10ms);
            }
        }
        const std::vector<std::string> parts{
            "[模拟回答] ", model + "：", "已收到", "你的问题。", "（仅用于联调）"};
        if (!stream) {
            if (scenario == "bad_json") { response.set_content("{invalid json", "application/json"); return; }
            std::string text;
            for (const auto& part : parts) text += part;
            Json value{{"id", "mock-completion"}, {"model", model}, {"object", "chat.completion"},
                {"choices", Json::array({{{"index", 0}, {"message", {{"role", "assistant"}, {"content", text}}},
                    {"finish_reason", "stop"}}})}};
            if (scenario == "length") value["choices"][0]["finish_reason"] = "length";
            if (scenario != "no_usage") value["usage"] = mock_usage;
            jsonReply(response, value);
            return;
        }
        ++active_streams;
        response.set_header("Cache-Control", "no-cache");
        response.set_chunked_content_provider("text/event-stream; charset=utf-8",
            [this, model, parts, scenario](std::size_t, httplib::DataSink& sink) {
                const auto write = [&](const std::string& bytes) {
                    if (scenario != "fragmented") return sink.write(bytes.data(), bytes.size());
                    // 故意拆开中文 UTF-8、data 字段以及 CRLF 分隔符。
                    for (std::size_t pos = 0; pos < bytes.size(); ++pos)
                        if (!sink.write(bytes.data() + pos, 1)) return false;
                    return true;
                };
                std::string packed;
                for (std::size_t i = 0; i < parts.size(); ++i) {
                    if (scenario == "heartbeats") {
                        for (int n = 0; n < 100; ++n) {
                            if (!write(": keep-alive\n\n")) return false;
                            std::this_thread::sleep_for(10ms);
                        }
                    }
                    if (scenario == "bad_json") {
                        if (!write("data: {invalid json\n\n")) return false;
                        sink.done(); return true;
                    }
                    const auto bytes = event(chunk(model, parts[i]));
                    if (scenario == "packed") packed += bytes;
                    else if (!write(bytes)) return false;
                    if (scenario == "disconnect" && i == 1) return false;
                    if (scenario == "stream_error" && i == 1) {
                        if (!write("event: error\ndata: {\"error\":\"mock failure\"}\n\n")) return false;
                        sink.done(); return true;
                    }
                    if (scenario == "stall" && i == 0) {
                        for (int n = 0; n < 200; ++n) {
                            if (!write(": waiting\n\n")) return false;
                            std::this_thread::sleep_for(10ms);
                        }
                    }
                    if (scenario != "packed") std::this_thread::sleep_for(std::chrono::milliseconds(segment_delay));
                }
                packed += event({{"model", model}, {"choices", Json::array({{{"index", 0},
                    {"delta", Json::object()}, {"finish_reason", "stop"}}})}});
                if (scenario != "no_usage")
                    packed += event({{"model", model}, {"choices", Json::array()}, {"usage", mock_usage}});
                if (scenario != "missing_done") packed += "data: [DONE]\r\n\r\n";
                if (!write(packed)) return false;
                sink.done();
                return true;
            }, [this](bool success) { --active_streams; if (!success) ++interrupted_streams; });
    }
};

MockHttpServer::MockHttpServer(int delay) : impl_(std::make_unique<Impl>(delay)) {}
MockHttpServer::~MockHttpServer() { stop(); }
int MockHttpServer::bind(int port) {
    if (port == 0) return impl_->server.bind_to_any_port("127.0.0.1");
    return impl_->server.bind_to_port("127.0.0.1", port) ? port : -1;
}
bool MockHttpServer::listen() { return impl_->server.listen_after_bind(); }
void MockHttpServer::waitUntilReady() const { impl_->server.wait_until_ready(); }
void MockHttpServer::stop() { impl_->server.stop(); }
}
