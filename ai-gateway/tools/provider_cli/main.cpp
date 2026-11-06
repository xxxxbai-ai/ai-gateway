#include "providers/compatible_chat_adapter.h"
#include "common/console.h"
#include <nlohmann/json.hpp>
#include <csignal>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
using Json = nlohmann::json;
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
Json readJson(const std::string& path) {
    std::ifstream file(std::filesystem::u8path(path));
    if (!file) throw std::runtime_error("无法打开配置或消息文件");
    return Json::parse(file);
}
Json usageJson(const std::optional<gateway::Usage>& usage) {
    if (!usage) return nullptr;
    const auto number = [](const auto& value) -> Json { return value ? Json(*value) : Json(nullptr); };
    return {{"input_tokens", number(usage->input_tokens)}, {"output_tokens", number(usage->output_tokens)},
        {"total_tokens", number(usage->total_tokens)}, {"simulated", usage->simulated}};
}
// Ctrl+C 或定时取消均设置同一标志，供 A 集成时照此传入。
struct CancellationWatcher {
    std::atomic_bool finished{false};
    std::thread worker;
    CancellationWatcher(gateway::RequestContext& context, int after_ms) {
        worker = std::thread([this, &context, after_ms] {
            const auto start = gateway::RequestContext::Clock::now();
            while (!finished.load()) {
                if (interrupted || (after_ms >= 0 && gateway::RequestContext::Clock::now() - start >= std::chrono::milliseconds(after_ms))) {
                    context.cancelled->store(true);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    ~CancellationWatcher() { finished.store(true); worker.join(); }
};
}

int main(int argc, char** argv) {
    gateway::enableUtf8Console();
    try {
        const auto arguments = gateway::utf8Arguments(argc, argv);
        gateway::ChatRequest request{"介绍一下计算机缓存", "demo-code"};
        gateway::ModelConfig model;
        model.id = "demo-code";
        model.base_url = "http://127.0.0.1:18080/v1";
        model.simulated = true;
        gateway::RequestContext context;
        std::string config, base_url, provider_model, key_env, message_file, record;
        int total_ms = 120000, cancel_ms = -1;
        bool json_only = false, force_simulated = false;
        for (std::size_t i = 1; i < arguments.size(); ++i) {
            const std::string arg = arguments[i];
            if (arg == "--help") {
                std::cout << "provider_cli [--stream] [--model demo-code] [--message 文本]\n"
                    << "  --config 文件           读取 models 配置数组\n"
                    << "  --messages 文件         UTF-8 JSON 消息数组，保留完整历史\n"
                    << "  --base-url 地址         API 根地址，自动追加 /chat/completions\n"
                    << "  --provider-model 名称   上游模型名，默认使用网关模型 ID\n"
                    << "  --key-env 变量名        从进程环境读取上游凭证\n"
                    << "  --max-tokens 数量       默认 50000\n"
                    << "  --connect-timeout-ms N  默认 5000\n"
                    << "  --first-timeout-ms N    默认 30000\n"
                    << "  --timeout-ms N          默认 120000，整个调用共用\n"
                    << "  --cancel-after-ms N     N 毫秒后取消；也支持 Ctrl+C\n"
                    << "  --json                 只输出最终 JSON\n"
                    << "  --simulated            将自定义地址标为模拟服务\n"
                    << "  --record 文件          追加诊断 JSONL，不保存问题/回答/凭证\n"
                    << "  --request-id 编号      指定联调请求编号\n"
                    << "故障示例：--base-url http://127.0.0.1:18080/scenario/disconnect/v1 --stream --simulated\n";
                return 0;
            }
            if (arg == "--stream") { request.stream = true; continue; }
            if (arg == "--json") { json_only = true; continue; }
            if (arg == "--simulated") { force_simulated = true; continue; }
            if (i + 1 >= arguments.size()) throw std::runtime_error("参数缺少值");
            const std::string value = arguments[++i];
            if (arg == "--config") config = value;
            else if (arg == "--model") model.id = value;
            else if (arg == "--message") request.question = value;
            else if (arg == "--messages") message_file = value;
            else if (arg == "--base-url") base_url = value;
            else if (arg == "--provider-model") provider_model = value;
            else if (arg == "--key-env") key_env = value;
            else if (arg == "--request-id") context.request_id = value;
            else if (arg == "--record") record = value;
            else if (arg == "--max-tokens") request.max_tokens = std::stoi(value);
            else if (arg == "--timeout-ms") total_ms = std::stoi(value);
            else if (arg == "--connect-timeout-ms") context.connect_timeout = std::chrono::milliseconds(std::stoi(value));
            else if (arg == "--first-timeout-ms") context.first_response_timeout = std::chrono::milliseconds(std::stoi(value));
            else if (arg == "--cancel-after-ms") cancel_ms = std::stoi(value);
            else throw std::runtime_error("未知参数，使用 --help 查看用法");
        }
        if (!config.empty()) {
            bool found = false;
            const auto configuration = readJson(config);
            for (const auto& value : configuration.at("models")) {
                if (value.at("id").get<std::string>() != model.id) continue;
                model.base_url = value.at("base_url").get<std::string>();
                model.provider_model = value.value("provider_model", model.id);
                model.credential_ref = value.value("credential_ref", "");
                model.protocol = value.value("protocol", "chat_completions");
                model.enabled = value.value("enabled", true);
                model.simulated = value.value("simulated", false);
                found = true; break;
            }
            if (!found) throw std::runtime_error("配置中没有指定模型");
        }
        if (!base_url.empty()) { model.base_url = base_url; model.simulated = false; }
        if (force_simulated) model.simulated = true;
        if (!provider_model.empty()) model.provider_model = provider_model;
        if (!key_env.empty()) model.credential_ref = "env:" + key_env;
        request.model = model.id;
        if (!message_file.empty()) {
            for (const auto& value : readJson(message_file))
                request.messages.push_back({value.at("role").get<std::string>(), value.at("content").get<std::string>()});
        }
        if (total_ms <= 0 || context.connect_timeout.count() <= 0 || context.first_response_timeout.count() <= 0)
            throw std::runtime_error("超时时间必须为正数");
        if (context.request_id.empty()) {
            const auto id = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            context.request_id = "d-" + std::to_string(id);
        }
        context.deadline = gateway::RequestContext::Clock::now() + std::chrono::milliseconds(total_ms);
        std::signal(SIGINT, interrupt);
        CancellationWatcher watcher(context, cancel_ms);
        gateway::CompatibleChatAdapter adapter;
        const auto result = adapter.invoke(model, request, context, [&](const gateway::ProviderEvent& event) {
            if (!json_only && event.kind == gateway::EventKind::Delta)
                std::cout << event.text << std::flush;
            return true;
        });
        Json summary{{"request_id", result.request_id}, {"success", result.ok()},
            {"model_id", result.model_id}, {"provider_model", result.provider_model},
            {"http_status", result.http_status}, {"elapsed_ms", result.elapsed_ms},
            {"completed", result.completed}, {"explicit_rejection", result.explicit_rejection},
            {"error_code", gateway::errorName(result.error_code)}, {"error_message", result.error_message},
            {"usage", usageJson(result.usage)}, {"simulated", model.simulated}};
        if (!record.empty()) {
            std::ofstream file(std::filesystem::u8path(record), std::ios::app);
            if (!file || !(file << summary.dump() << '\n')) throw std::runtime_error("无法写入调用记录");
        }
        summary["answer"] = result.answer;
        if (!json_only) std::cout << '\n';
        std::cout << summary.dump(2) << '\n';
        return result.ok() ? 0 : 1;
    } catch (const Json::exception&) {
        // JSON 解析异常可能包含原文，避免打印意外写入配置的凭证。
        std::cerr << "配置或消息不是有效的 UTF-8 JSON；检查字段及 --help。\n";
        return 2;
    } catch (const std::exception& error) {
        std::cerr << "错误：" << error.what() << '\n';
        return 2;
    }
}
