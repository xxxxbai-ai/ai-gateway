#include "shared/module_api.h"
#include "mock_server.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace api = gateway_contract;
using namespace std::chrono_literals;
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Fixture {
    gateway::MockHttpServer server{1};
    int port = server.bind();
    std::thread worker;
    Fixture() {
        require(port > 0, "模拟服务绑定失败");
        worker = std::thread([this] { server.listen(); });
        server.waitUntilReady();
    }
    ~Fixture() { server.stop(); worker.join(); }
    api::ModelConfig model(const std::string& scenario = "normal") {
        api::ModelConfig model;
        model.id = "mock_code";
        model.provider_model = "demo-code";
        model.base_url = "http://127.0.0.1:" + std::to_string(port) + "/scenario/" + scenario + "/v1";
        return model;
    }
    nlohmann::json stats() {
        httplib::Client client("127.0.0.1", port);
        auto response = client.Get("/stats");
        require(response && response->status == 200, "统计请求失败");
        return nlohmann::json::parse(response->body);
    }
    int calls() { return stats().at("counts").at("demo-code").get<int>(); }
};

int main() {
    try {
        Fixture fixture;
        api::ChatRequest request;
        request.message = "请解释 C++ 的引用。";
        api::RequestContext context;
        context.request_id = "public-d-001";
        const auto original_deadline = context.deadline;
        const auto original_cancel = context.cancelled;
        auto result = api::callModel(fixture.model(), request, context);
        require(result.success && result.error.code == api::ErrorCode::None && result.error.message.empty(), "公共调用未成功");
        require(result.answer.find("demo-code") != std::string::npos && result.model_id == "mock_code" &&
            result.request_id == "public-d-001" && result.http_status == 200, "公共编号与上游编号转换错误");
        require(context.deadline == original_deadline && context.cancelled == original_cancel, "上下文被重置");
        require(fixture.calls() == 1, "一次公共调用必须只发一次上游请求");
        require(fixture.stats().at("last_max_tokens") == 50000, "公共调用未传递 50000 token 上限");
        std::cout << "实际模拟输入：" << request.message << "\n实际模拟回答：" << result.answer << '\n';

        request.mode = "manual";
        request.model = "mock_code";
        result = api::callModel(fixture.model(), request, context);
        require(result.success, "手动调用失败");
        request.mode = "auto";
        request.model.reset();
        for (const auto scenario : {"reject429", "reject503", "unauthorized", "bad_json", "length"}) {
            const int before = fixture.calls();
            result = api::callModel(fixture.model(scenario), request, context);
            const std::string kind = scenario;
            const auto code = kind == "bad_json" ? api::ErrorCode::InvalidResponse :
                kind == "length" ? api::ErrorCode::IncompleteResponse :
                kind == "unauthorized" ? api::ErrorCode::UpstreamError : api::ErrorCode::UpstreamRejected;
            require(!result.success && result.error.code == code && !result.error.message.empty(), "错误映射不正确");
            require(result.model_id == "mock_code" && result.request_id == context.request_id, "失败结果丢失编号");
            require(fixture.calls() == before + 1, "失败后 D 不得重试");
            const bool rejected = kind == "reject429" || kind == "reject503";
            require(result.explicit_rejection == rejected, "明确拒绝事实错误");
            require(api::mayUseFallback(request, result, context, 1) == rejected, "备用判断不符合公共规则");
        }
        const int before = fixture.calls();
        context.cancelled->store(true);
        result = api::callModel(fixture.model(), request, context);
        require(result.error.code == api::ErrorCode::Cancelled && result.http_status == 0, "取消标志未复用");
        context.cancelled->store(false);
        context.deadline = api::RequestContext::Clock::now() - 1ms;
        result = api::callModel(fixture.model(), request, context);
        require(result.error.code == api::ErrorCode::TotalTimeout, "公共截止时间未复用");
        context.deadline = api::RequestContext::Clock::now() + 5s;
        auto model = fixture.model();
        model.enabled = false;
        require(api::callModel(model, request, context).error.code == api::ErrorCode::DisabledModel, "禁用模型错误");
        model = fixture.model();
        model.is_mock = false;
        require(api::callModel(model, request, context).error.code == api::ErrorCode::InvalidConfig, "真实模型必须使用 HTTPS");
        model.base_url = "https://api.deepseek.com";
        require(api::callModel(model, request, context).error.code == api::ErrorCode::MissingCredential, "真实模型必须有凭证引用");
        model.credential_ref = "env:GATEWAY_D_TEST_MISSING_CREDENTIAL_20261027";
        require(api::callModel(model, request, context).error.code == api::ErrorCode::MissingCredential, "缺失环境变量未映射");
        request.mode = "manual";
        request.model = "another_model";
        require(api::callModel(fixture.model(), request, context).error.code == api::ErrorCode::InvalidRequest, "手动编号冲突应拒绝");
        require(fixture.calls() == before, "本地错误不应访问上游");
        request.mode = "auto";
        request.model.reset();
        context.first_response_timeout = 80ms;
        result = api::callModel(fixture.model("delay"), request, context);
        require(result.error.code == api::ErrorCode::FirstResponseTimeout && result.elapsed_ms < 1000, "首响应超时未复用");
        context.first_response_timeout = 2s;
        context.deadline = api::RequestContext::Clock::now() + 80ms;
        result = api::callModel(fixture.model("delay"), request, context);
        require(result.error.code == api::ErrorCode::TotalTimeout && result.elapsed_ms < 1000, "总预算被重置");
        std::cout << "公共 callModel：成功、单次调用、错误、取消、截止时间及超时验证通过。\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
