#include "providers/compatible_chat_adapter.h"
#include "providers/sse_decoder.h"
#include "server/gateway_service.h"
#include "mock_server.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using namespace gateway;
using namespace std::chrono_literals;
using Json = nlohmann::json;
void require(bool passed, const std::string& message) {
    if (!passed) throw std::runtime_error(message);
}
struct Fixture {
    MockHttpServer server{10};
    int port;
    std::thread worker;
    Fixture() : port(server.bind()) {
        require(port > 0, "无法绑定模拟端口");
        worker = std::thread([this] { server.listen(); });
        server.waitUntilReady();
    }
    ~Fixture() { server.stop(); worker.join(); }
    ModelConfig model(const std::string& scenario = "normal", const std::string& id = "demo-code") const {
        ModelConfig result;
        result.id = id;
        result.provider_model = id;
        result.simulated = true;
        result.base_url = "http://127.0.0.1:" + std::to_string(port) + "/scenario/" + scenario + "/v1";
        return result;
    }
    Json stats() const {
        auto* handle = curl_easy_init();
        require(handle != nullptr, "curl 初始化失败");
        const auto url = "http://127.0.0.1:" + std::to_string(port) + "/stats";
        std::string body;
        curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(handle, CURLOPT_NOPROXY, "*");
        curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 2000L);
        curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, +[](char* bytes, std::size_t size, std::size_t n, void* output) -> std::size_t {
            static_cast<std::string*>(output)->append(bytes, size * n); return size * n;
        });
        curl_easy_setopt(handle, CURLOPT_WRITEDATA, &body);
        const auto code = curl_easy_perform(handle);
        curl_easy_cleanup(handle);
        require(code == CURLE_OK, "统计接口不可用");
        return Json::parse(body);
    }
};
ChatRequest request(bool stream = false) {
    ChatRequest result{"请解释中文 UTF-8 网络分片", "demo-code"};
    result.stream = stream;
    return result;
}

void sseTests() {
    // 每个可能拆包位置都试一次，包含汉字内部、CRLF 中间和事件边界。
    const std::string input = ":注释\r\ndata: 你好\r\ndata: 世界\r\n\r\nevent: custom\ndata: 第二条\n\n";
    for (std::size_t cut = 0; cut <= input.size(); ++cut) {
        SseDecoder decoder;
        std::vector<std::pair<std::string, std::string>> events;
        auto collect = [&](const auto& type, const auto& data) { events.emplace_back(type, data); return true; };
        require(decoder.feed(std::string_view(input).substr(0, cut), collect), "首包解析失败");
        require(decoder.feed(std::string_view(input).substr(cut), collect), "尾包解析失败");
        require(events.size() == 2 && events[0].second == "你好\n世界" &&
            events[1].first == "custom" && events[1].second == "第二条", "拆包或多 data 行解析错误");
    }
    SseDecoder decoder;
    int events = 0;
    for (char ch : input) require(decoder.feed(std::string_view(&ch, 1), [&](const auto&, const auto&) { ++events; return true; }), "逐字节失败");
    require(events == 2, "逐字节事件丢失");
    require(decoder.feed("data: 未完成", [&](const auto&, const auto&) { ++events; return true; }), "缓存失败");
    require(events == 2, "不完整事件不应提前输出");
}

void callTests(Fixture& fixture) {
    CompatibleChatAdapter adapter;
    for (const std::string id : {"demo-code", "demo-language", "demo-general"}) {
        RequestContext context;
        context.request_id = "test-" + id;
        const auto result = adapter.invoke(fixture.model("normal", id), request(), context);
        require(result.ok() && result.answer.find(id) != std::string::npos, "模型未真实收到请求");
        require(result.request_id == context.request_id && result.model_id == id, "请求或模型编号丢失");
        require(result.usage && result.usage->total_tokens == 32 && result.usage->simulated, "模拟用量缺失");
    }
    auto messages = request();
    messages.messages = {{"system", "使用中文"}, {"user", "给出代码"}, {"assistant", "上轮回答"}, {"user", "改成递归"}};
    messages.max_tokens = 77;
    auto alias = fixture.model("normal", "gateway-alias");
    alias.provider_model = "demo-language";
    RequestContext context;
    const auto result = adapter.invoke(alias, messages, context);
    require(result.ok() && result.model_id == "gateway-alias" && result.provider_model == "demo-language", "上游名称映射错误");
    const auto stats = fixture.stats();
    require(stats["last_message_count"] == 4 && stats["last_max_tokens"] == 77, "消息历史或输出上限未传递");
    require(stats["counts"]["demo-code"] == 1 && stats["counts"]["demo-language"] == 2 && stats["counts"]["demo-general"] == 1,
        "请求次数不符合实际调用");
    auto missing = fixture.model();
    missing.credential_ref = "env:GATEWAY_TEST_MISSING_CREDENTIAL_76C927A4";
    RequestContext no_key;
    require(adapter.invoke(missing, request(), no_key).error_code == ProviderErrorCode::MissingCredential, "缺失凭证没有明确错误");
    auto invalid = request(); invalid.question.clear();
    RequestContext invalid_context;
    require(adapter.invoke(fixture.model(), invalid, invalid_context).error_code == ProviderErrorCode::InvalidRequest, "空消息未拒绝");
    invalid.question = std::string(1, static_cast<char>(0xff));
    RequestContext invalid_utf8;
    require(adapter.invoke(fixture.model(), invalid, invalid_utf8).error_code == ProviderErrorCode::InvalidRequest, "非法 UTF-8 未转换成业务错误");
    require(fixture.stats()["counts"] == stats["counts"], "校验失败仍调用了上游");
}

void streamTests(Fixture& fixture) {
    CompatibleChatAdapter adapter;
    for (const std::string scenario : {"normal", "fragmented", "packed", "no_usage"}) {
        RequestContext context;
        std::string assembled;
        int deltas = 0, finishes = 0;
        RequestContext::Clock::time_point first_delta;
        const auto result = adapter.invoke(fixture.model(scenario), request(true), context, [&](const ProviderEvent& event) {
            if (event.kind == EventKind::Delta) {
                if (deltas == 0) first_delta = RequestContext::Clock::now();
                ++deltas; assembled += event.text;
            }
            if (event.kind == EventKind::Finish) ++finishes;
            return true;
        });
        require(result.ok() && deltas == 5 && finishes == 1 && assembled == result.answer, "流式段落丢失或重复：" + scenario);
        require(result.answer.find(request().question) != std::string::npos, "中文问题在流式回答中损坏");
        if (scenario == "normal")
            require(RequestContext::Clock::now() - first_delta >= 30ms, "等待完整回答后才输出首段");
        require((scenario == "no_usage") == !result.usage.has_value(), "未知用量被伪造");
    }
    RequestContext context;
    require(!adapter.invoke(fixture.model("no_usage"), request(), context).usage, "普通响应的缺失用量应保持未知");
}

void faultTests(Fixture& fixture) {
    CompatibleChatAdapter adapter;
    for (const std::string scenario : {"reject429", "reject503", "unauthorized"}) {
        RequestContext context;
        const int before = fixture.stats()["counts"]["demo-code"].get<int>();
        const auto result = adapter.invoke(fixture.model(scenario), request(true), context);
        require(!result.ok() && result.explicit_rejection == (scenario != "unauthorized"), "拒绝类型错误");
        require(fixture.stats()["counts"]["demo-code"].get<int>() == before + 1, "D 不应自动重试");
    }
    for (const std::string scenario : {"disconnect", "missing_done", "stream_error", "bad_json"}) {
        RequestContext context;
        int finishes = 0;
        const auto result = adapter.invoke(fixture.model(scenario), request(true), context, [&](const ProviderEvent& event) {
            if (event.kind == EventKind::Finish) ++finishes;
            return true;
        });
        const auto expected = scenario == "bad_json" ? ProviderErrorCode::InvalidResponse :
            scenario == "stream_error" ? ProviderErrorCode::UpstreamError : ProviderErrorCode::IncompleteResponse;
        require(!result.ok() && !result.completed && finishes == 0 && result.error_code == expected,
                "流式故障假报成功或错误分类不正确：" + scenario + " / " + errorName(result.error_code));
    }
    for (const std::string scenario : {"delay", "heartbeats"}) {
        RequestContext context; context.first_response_timeout = 80ms;
        const auto result = adapter.invoke(fixture.model(scenario), request(true), context);
        require(result.error_code == ProviderErrorCode::FirstResponseTimeout && result.elapsed_ms < 700,
            "首段超时无效，或心跳错误地重置了等待时间");
    }
    RequestContext shared;
    shared.deadline = RequestContext::Clock::now() + 150ms;
    const auto first = adapter.invoke(fixture.model("reject503"), request(true), shared);
    require(first.explicit_rejection, "首次拒绝缺少标记");
    std::this_thread::sleep_for(60ms);
    const auto second = adapter.invoke(fixture.model("stall"), request(true), shared);
    require(second.error_code == ProviderErrorCode::TotalTimeout && second.elapsed_ms < 150, "备用调用重置了总截止时间");
    RequestContext bad_json;
    require(adapter.invoke(fixture.model("bad_json"), request(), bad_json).error_code == ProviderErrorCode::InvalidResponse,
        "普通响应非法 JSON 未拒绝");
}

void cancelTests(Fixture& fixture) {
    CompatibleChatAdapter adapter;
    RequestContext before;
    before.cancelled->store(true);
    require(adapter.invoke(fixture.model(), request(), before).error_code == ProviderErrorCode::Cancelled, "预取消无效");
    require(fixture.stats()["counts"]["demo-code"] == 0, "预取消仍发起了调用");
    RequestContext context;
    int deltas = 0, finishes = 0;
    const auto result = adapter.invoke(fixture.model(), request(true), context, [&](const ProviderEvent& event) {
        if (event.kind == EventKind::Finish) ++finishes;
        if (event.kind == EventKind::Delta) return ++deltas < 2;
        return true;
    });
    require(result.error_code == ProviderErrorCode::Cancelled && !result.completed && finishes == 0 && deltas == 2,
        "回调取消未停止传输");
    RequestContext external;
    std::thread canceller([&] { std::this_thread::sleep_for(60ms); external.cancelled->store(true); });
    const auto pending = adapter.invoke(fixture.model("delay"), request(true), external);
    canceller.join();
    require(pending.error_code == ProviderErrorCode::Cancelled && pending.elapsed_ms < 700, "等待首段时无法及时取消");
    RequestContext throwing;
    const auto exception = adapter.invoke(fixture.model(), request(true), throwing, [](const auto&) -> bool {
        throw std::runtime_error("测试调用方异常");
    });
    require(exception.error_code == ProviderErrorCode::CallbackFailed, "回调异常跨过 C 边界");
    const auto deadline = RequestContext::Clock::now() + 1s;
    while (fixture.stats()["active_streams"] != 0 && RequestContext::Clock::now() < deadline)
        std::this_thread::sleep_for(20ms);
    require(fixture.stats()["active_streams"] == 0, "取消后模拟上游连接未释放");
}

std::vector<ModelConfig> gatewayModels(const Fixture& fixture) {
    auto models = ModelRepository().listModels();
    for (auto& model : models) model.base_url = fixture.model().base_url;
    return models;
}

void gatewayRouteTests(Fixture& fixture) {
    CompatibleChatAdapter adapter;
    GatewayService service(adapter, gatewayModels(fixture));
    const std::vector<std::pair<std::string, std::string>> examples{
        {"修复这段 C++ 代码", "demo-code"}, {"翻译这段 Python 注释", "demo-language"},
        {"介绍计算机缓存", "demo-general"}};
    for (const auto& example : examples) {
        ChatRequest input{example.first, "auto"}; input.stream = true;
        RequestContext context;
        std::string answer;
        const auto result = service.handle(input, context, [&](const ProviderEvent& event) {
            if (event.kind == EventKind::Delta) answer += event.text;
            return true;
        });
        require(result.ok() && result.attempts.size() == 1 && !result.fallback_used, "正常请求不应重复调用");
        const auto& actual = result.attempts.back();
        require(actual.model_id == example.second && actual.answer == answer && !context.request_id.empty(),
            "分析、路由和 HTTP 实际调用未串通");
    }
    const auto counts = fixture.stats()["counts"];
    require(counts["demo-code"] == 1 && counts["demo-language"] == 1 && counts["demo-general"] == 1,
        "三类问题没有到达三个独立模型");
    ChatRequest history{"", "auto"};
    history.messages = {{"user", "解释 C++ 代码"}, {"assistant", "上轮回答"}, {"user", "翻译成英文"}};
    RequestContext history_context;
    const auto continued = service.handle(history, history_context);
    require(continued.ok() && continued.attempts.back().model_id == "demo-language" &&
        fixture.stats()["last_message_count"] == 3, "网关没有保留完整消息历史");
    RequestContext manual_context;
    const auto manual = service.handle({"修复 C++ 代码", "demo-language"}, manual_context);
    require(manual.ok() && manual.attempts.back().model_id == "demo-language", "手动选模没有覆盖自动选模");
    const auto before = fixture.stats()["counts"];
    bool rejected = false;
    try { RequestContext context; service.handle({"普通问题", "nonexistent"}, context); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected && fixture.stats()["counts"] == before, "不存在的模型仍然调用了上游");
}

void gatewayFallbackTests(Fixture& fixture) {
    CompatibleChatAdapter adapter;
    auto models = gatewayModels(fixture);
    models[0].base_url = fixture.model("reject503").base_url;
    ChatRequest input{"修复 C++ 代码", "auto"}; input.stream = true;
    RequestContext context;
    context.request_id = "gateway-fallback-test";
    std::string answer;
    const auto switched = GatewayService(adapter, models).handle(input, context, [&](const ProviderEvent& event) {
        if (event.kind == EventKind::Delta) answer += event.text;
        return true;
    });
    require(switched.ok() && switched.fallback_used && switched.attempts.size() == 2 &&
        switched.decision.model_id == "demo-code" && switched.attempts.back().model_id == "demo-general",
        "明确拒绝后没有使用正确备用模型");
    require(answer.find("demo-general") != std::string::npos && answer.find("demo-code") == std::string::npos,
        "备用回答与失败回答混在一起");
    require(switched.attempts[0].request_id == context.request_id && switched.attempts[1].request_id == context.request_id,
        "同一请求的尝试编号不一致");

    auto manual_input = input; manual_input.model = "demo-code";
    RequestContext manual_context;
    const auto manual = GatewayService(adapter, models).handle(manual_input, manual_context);
    require(!manual.ok() && !manual.fallback_used && manual.attempts.size() == 1, "手动请求被自动换模型");

    models[2].base_url = fixture.model("reject429").base_url;
    const auto before_third = fixture.stats()["counts"]["demo-language"];
    RequestContext twice;
    const auto failed_twice = GatewayService(adapter, models).handle(input, twice);
    require(!failed_twice.ok() && failed_twice.attempts.size() == 2 &&
        fixture.stats()["counts"]["demo-language"] == before_third, "不应尝试第三个模型");

    models[1].enabled = models[2].enabled = false;
    RequestContext alone;
    const auto unavailable = GatewayService(adapter, models).handle(input, alone);
    require(!unavailable.fallback_used && unavailable.attempts.size() == 1, "没有可用备用时不应再调用");

    for (const std::string scenario : {"unauthorized", "disconnect", "delay"}) {
        auto single = gatewayModels(fixture);
        single[0].base_url = fixture.model(scenario).base_url;
        RequestContext no_retry; no_retry.first_response_timeout = 80ms;
        const auto failure = GatewayService(adapter, single).handle(input, no_retry);
        require(!failure.ok() && !failure.fallback_used && failure.attempts.size() == 1,
            "鉴权错误、断流或超时不应自动备用：" + scenario);
    }
    auto normal = gatewayModels(fixture);
    RequestContext cancelled;
    const auto cancellation = GatewayService(adapter, normal).handle(input, cancelled, [](const auto&) { return false; });
    require(cancellation.attempts.size() == 1 && !cancellation.fallback_used &&
        cancellation.attempts.back().error_code == ProviderErrorCode::Cancelled, "用户取消后不应备用");

    normal[0].base_url = fixture.model("reject503").base_url;
    normal[2].base_url = fixture.model("stall").base_url;
    RequestContext shared_deadline;
    const auto deadline = RequestContext::Clock::now() + 120ms;
    shared_deadline.deadline = deadline;
    const auto timed_out = GatewayService(adapter, normal).handle(input, shared_deadline);
    require(timed_out.attempts.size() == 2 && timed_out.attempts.back().error_code == ProviderErrorCode::TotalTimeout &&
        shared_deadline.deadline == deadline, "网关备用重置了总时限");

    // 即使未来适配器报告拒绝，只要已经输出过内容，网关也不能拼接其他模型。
    struct StartedThenRejected : ProviderAdapter {
        AttemptResult invoke(const ModelConfig& model, const ChatRequest&, RequestContext&, EventSink sink) override {
            if (sink) sink({EventKind::Delta, "已经开始的回答"});
            AttemptResult result;
            result.model_id = model.id;
            result.http_status = 503;
            result.explicit_rejection = true;
            result.error_code = ProviderErrorCode::UpstreamRejected;
            return result;
        }
    } started_provider;
    RequestContext started_context;
    const auto started = GatewayService(started_provider, gatewayModels(fixture)).handle(input, started_context,
        [](const auto&) { return true; });
    require(started.attempts.size() == 1 && !started.fallback_used, "已经输出的请求仍然发生备用");
}
}

int main(int argc, char** argv) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 1;
    int status = 0;
    try {
        require(argc == 2, "缺少测试组名称");
        const std::string group = argv[1];
        if (group == "sse") sseTests();
        else {
            Fixture fixture;
            if (group == "calls") callTests(fixture);
            else if (group == "stream") streamTests(fixture);
            else if (group == "faults") faultTests(fixture);
            else if (group == "cancel") cancelTests(fixture);
            else if (group == "gateway_route") gatewayRouteTests(fixture);
            else if (group == "gateway_fallback") gatewayFallbackTests(fixture);
            else throw std::runtime_error("未知测试组");
        }
        std::cout << "PASS " << group << '\n';
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        status = 1;
    }
    curl_global_cleanup();
    return status;
}
