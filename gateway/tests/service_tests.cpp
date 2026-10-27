#include "../service.h"
#include "../cli_arguments.h"
#include "test_doubles.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace api = gateway_contract;
using Json = nlohmann::json;
namespace {
void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
std::string question(const std::string& mode = "auto", Json model = nullptr) {
    return Json{{"mode", mode}, {"model", model}, {"message", "  解释 C++ 指针 \n"}}.dump();
}
api::CallResult rejected(long status = 503) {
    api::CallResult value;
    value.http_status = status;
    value.explicit_rejection = true;
    value.error = {api::ErrorCode::UpstreamRejected, "预设拒绝"};
    return value;
}
}

int main(int argc, char** argv) {
    const auto args = app::arguments(argc, argv);
    if (args.size() != 2) return 2;
    const auto root = std::filesystem::u8path(args[1]);
    std::filesystem::create_directories(root);
    int passed = 0, failed = 0, sequence = 0;
    const auto run = [&](const char* name, const std::function<void(app::Service&, const std::filesystem::path&)>& check) {
        a_test::reset();
        const auto directory = root / ("case-" + std::to_string(++sequence));
        std::filesystem::create_directories(directory);
        const auto log = directory / "requests.jsonl";
        try {
            app::Service service((directory / "C-provided-config.json").u8string(), log.u8string());
            check(service, log);
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    };
    run("forward input and actual results", [](auto& service, const auto& log) {
        const auto response = service.chat(question());
        const auto& fixture = a_test::state();
        require(response.status == 200 && response.body["success"] == true, "success response");
        require(fixture.analyses == 1 && fixture.routes == 1 && fixture.calls.size() == 1, "B/C/D invocation counts");
        require(fixture.routed_analysis.task == fixture.analysis.task, "B output forwarded to C");
        require(fixture.calls[0].message == "解释 C++ 指针", "trim and Unicode forwarding");
        require(response.body["selected_model"] == fixture.calls[0].model &&
                response.body["request_id"] == fixture.calls[0].request_id, "actual model/request identity");
        require(response.body["answer"] == "TEST DOUBLE ONLY: 解释 C++ 指针", "actual D answer");
        std::ifstream input(log);
        Json record; input >> record;
        require(record["attempts"].size() == 1 && record.contains("elapsed_ms"), "request log");
        require(record.dump().find("解释 C++ 指针") == std::string::npos, "log excludes question");
    });
    for (const long status : {429, 503}) run(status == 429 ? "fallback after 429" : "fallback after 503", [status](auto& service, const auto&) {
        a_test::state().outcomes = {rejected(status)};
        const auto response = service.chat(question());
        const auto& calls = a_test::state().calls;
        require(response.status == 200 && response.body["fallback_used"] == true, "fallback response");
        require(calls.size() == 2 && calls[0].model != calls[1].model &&
                calls[0].request_id == calls[1].request_id && calls[0].deadline == calls[1].deadline &&
                calls[0].cancelled == calls[1].cancelled, "two different models share request/deadline/cancellation");
        require(response.body["selected_model"] == calls[1].model, "final successful model");
    });
    run("manual failure never switches", [](auto& service, const auto&) {
        a_test::state().outcomes = {rejected()};
        const auto response = service.chat(question("manual", "a_test_primary"));
        require(response.status == 503 && a_test::state().calls.size() == 1, "manual single attempt");
        require(a_test::state().routed_request.model == std::optional<std::string>("a_test_primary"), "manual id forwarded");
    });
    run("two failures never call a third", [](auto& service, const auto&) {
        a_test::state().outcomes = {rejected(), rejected(429)};
        require(service.chat(question()).status == 503 && a_test::state().calls.size() == 2, "two-attempt limit");
    });
    for (const auto code : {api::ErrorCode::ConnectionFailed, api::ErrorCode::TotalTimeout, api::ErrorCode::InvalidResponse})
        run(api::errorCodeName(code), [code](auto& service, const auto&) {
            api::CallResult outcome; outcome.error = {code, "预设错误"};
            a_test::state().outcomes = {outcome};
            const auto response = service.chat(question());
            require(response.status == api::httpStatus(code) && a_test::state().calls.size() == 1, "error mapping without fallback");
            require(!response.body.contains("answer"), "failed answer not exposed as success");
        });
    run("503 alone is insufficient", [](auto& service, const auto&) {
        auto outcome = rejected(); outcome.explicit_rejection = false;
        a_test::state().outcomes = {outcome};
        require(service.chat(question()).status == 503 && a_test::state().calls.size() == 1, "explicit rejection required");
    });
    run("missing backup", [](auto& service, const auto&) {
        a_test::state().route.fallback_model.reset(); a_test::state().outcomes = {rejected()};
        require(service.chat(question()).status == 503 && a_test::state().calls.size() == 1, "no implicit backup");
    });
    run("duplicate backup", [](auto& service, const auto&) {
        a_test::state().route.fallback_model = "a_test_primary"; a_test::state().outcomes = {rejected()};
        require(service.chat(question()).status == 503 && a_test::state().calls.size() == 1, "backup must differ");
    });
    for (const bool cancel : {false, true}) run(cancel ? "cancel prevents fallback" : "deadline prevents fallback", [cancel](auto& service, const auto&) {
        a_test::state().outcomes = {rejected()};
        a_test::state().expire_after_call = !cancel; a_test::state().cancel_after_call = cancel;
        require(service.chat(question()).status == 503 && a_test::state().calls.size() == 1, "shared deadline/cancellation respected");
    });
    run("invalid requests stop before modules", [](auto& service, const auto&) {
        for (const auto& body : std::vector<std::string>{"{broken", "[]", "{}",
            R"({"mode":true,"model":null,"message":"x"})",
            R"({"mode":"auto","model":"x","message":"x"})",
            R"({"mode":"manual","model":null,"message":"x"})",
            R"({"mode":"auto","model":null,"message":"  "})",
            R"({"mode":"auto","model":null,"message":"x","demo_failure":true})"})
            require(service.chat(body).status == 400, "input rejected");
        require(a_test::state().analyses == 0 && a_test::state().calls.empty(), "no B/D on invalid input");
    });
    run("configuration failure", [](auto& service, const auto&) {
        a_test::state().load_error = {api::ErrorCode::InvalidConfig, "预设配置错误"};
        require(service.chat(question()).status == 500 && service.models().status == 500, "C failure mapping");
        require(a_test::state().calls.empty(), "D not called");
    });
    run("routing failure", [](auto& service, const auto&) {
        a_test::state().route = {false, "", {}, "", {api::ErrorCode::UnknownModel, "预设不存在"}};
        require(service.chat(question("manual", "unknown")).status == 400 && a_test::state().calls.empty(), "route error forwarded");
    });
    run("safe settings and model list", [](auto& service, const auto&) {
        a_test::state().models[1].enabled = false;
        require(service.models().body["models"].size() == 1, "only enabled models exposed");
        const auto config = service.settings();
        require(config.body["models"].size() == 2 && !config.body["models"][0].contains("base_url") &&
                !config.body["models"][0].contains("credential_ref"), "settings omit private connection fields");
    });
    run("partial settings forwarded to C", [](auto& service, const auto&) {
        auto entry = service.settings().body["models"][0];
        entry["scores"]["programming"] = 100;
        const auto before = a_test::state().models;
        const auto response = service.saveSettings(Json{{"models", Json::array({entry})}}.dump());
        require(response.status == 200 && a_test::state().saves == 1, "save invoked once");
        require(a_test::state().models[0].scores.at("programming") == 100 &&
                a_test::state().models[0].credential_ref == before[0].credential_ref &&
                a_test::state().models[1].scores == before[1].scores, "only requested public values changed");
    });
    run("invalid settings never saved", [](auto& service, const auto&) {
        auto entry = service.settings().body["models"][0]; entry["scores"]["math"] = 101;
        require(service.saveSettings(Json{{"models", Json::array({entry})}}.dump()).status == 400 &&
                a_test::state().saves == 0, "invalid score rejected before C save");
    });
    run("save failure forwarded", [](auto& service, const auto&) {
        a_test::state().save_error = {api::ErrorCode::InvalidConfig, "预设保存失败"};
        auto entries = service.settings().body["models"];
        require(service.saveSettings(Json{{"models", entries}}.dump()).status == 500, "save error not reported as success");
    });
    run("exception becomes structured failure", [](auto& service, const auto&) {
        a_test::state().throw_analysis = true;
        const auto response = service.chat(question());
        require(response.status == 500 && response.body["error_code"] == "internal_error", "exception response");
    });
    std::cout << "A gateway checks: " << passed << " passed, " << failed << " failed; B/C/D are test doubles only.\n";
    return failed ? 1 : 0;
}
