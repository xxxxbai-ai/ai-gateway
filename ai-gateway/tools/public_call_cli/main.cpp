#include "shared/module_api.h"
#include "common/console.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

// 从仓库根目录运行：ai-gateway/build-d/public_call_cli --list
// --model qwen_plus 指定模型；密钥按 credential_ref 从环境读取，不保存在配置中。
int main(int argc, char** argv) {
    gateway::enableUtf8Console();
    using Json = nlohmann::json;
    namespace api = gateway_contract;
    try {
        std::string config = "config/models.providers.example.json";
        std::string model_id = "deepseek_flash";
        bool list_models = false;
        api::ChatRequest request;
        request.message = "请用一句话解释什么是 API 网关。";
        api::RequestContext context;
        context.request_id = "d-" + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        const auto arguments = gateway::utf8Arguments(argc, argv);
        for (std::size_t i = 1; i < arguments.size(); ++i) {
            const auto& arg = arguments[i];
            if (arg == "--help") {
                std::cout << "public_call_cli [--config 文件] [--model 公共编号] [--message 问题] [--list]\n"
                    << "默认配置：config/models.providers.example.json\n"
                    << "--list 仅列出模型、启用状态和进程环境中的凭证是否已配置，不发出模型请求。\n"
                    << "新厂商模板默认禁用；开通服务并配置密钥后，将对应 enabled 改为 true。\n";
                return 0;
            }
            if (arg == "--list") { list_models = true; continue; }
            if (i + 1 >= arguments.size()) throw std::runtime_error("参数缺少值");
            const auto& value = arguments[++i];
            if (arg == "--config") config = value;
            else if (arg == "--model") model_id = value;
            else if (arg == "--message") request.message = value;
            else throw std::runtime_error("未知参数");
        }
        std::ifstream file(std::filesystem::u8path(config));
        if (!file) throw std::runtime_error("无法打开模型配置文件");
        const auto root = Json::parse(file);
        if (root.at("contract_version") != "1.0") throw std::runtime_error("配置版本应为 1.0");
        if (list_models) {
            auto models = Json::array();
            for (const auto& value : root.at("models")) {
                const auto reference = value.at("credential_ref").get<std::string>();
                const auto variable = reference.rfind("env:", 0) == 0 ? reference.substr(4) : "";
                const char* secret = variable.empty() ? nullptr : std::getenv(variable.c_str());
                models.push_back({{"id", value.at("id")}, {"name", value.at("name")},
                    {"enabled", value.at("enabled")}, {"provider_model", value.at("provider_model")},
                    {"credential_env", variable}, {"credential_configured", secret && *secret}});
            }
            std::cout << Json{{"models", models}}.dump(2) << '\n';
            return 0;
        }
        api::ModelConfig model;
        bool found = false;
        for (const auto& value : root.at("models")) {
            if (value.at("id").get<std::string>() != model_id) continue;
            model.id = model_id;
            model.name = value.at("name").get<std::string>();
            model.enabled = value.at("enabled").get<bool>();
            model.is_mock = value.at("is_mock").get<bool>();
            model.provider_model = value.at("provider_model").get<std::string>();
            model.base_url = value.at("base_url").get<std::string>();
            model.credential_ref = value.at("credential_ref").get<std::string>();
            model.protocol = value.at("protocol").get<std::string>();
            found = true;
            break;
        }
        if (!found) throw std::runtime_error("配置中没有指定模型");
        request.mode = "manual";
        request.model = model.id;
        context.deadline = api::RequestContext::Clock::now() + std::chrono::seconds(120);
        const auto result = api::callModel(model, request, context);
        std::cout << Json{{"success", result.success}, {"request_id", result.request_id},
            {"model_id", result.model_id}, {"provider_model", model.provider_model},
            {"is_mock", model.is_mock}, {"answer", result.answer},
            {"elapsed_ms", result.elapsed_ms}, {"http_status", result.http_status},
            {"explicit_rejection", result.explicit_rejection},
            {"error_code", api::errorCodeName(result.error.code)}, {"error", result.error.message}}.dump(2) << '\n';
        return result.success ? 0 : 1;
    } catch (const Json::exception&) {
        std::cerr << "模型配置格式错误，请检查 JSON 及字段类型。\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
    }
    return 2;
}
