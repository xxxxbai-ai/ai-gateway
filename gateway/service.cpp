#include "service.h"
#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace app {
using Json = nlohmann::json;
namespace api = gateway_contract;
namespace {
std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n\f\v");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n\f\v") - first + 1);
}
std::string newId() {
    static std::atomic<unsigned long long> sequence{0};
    static const auto started = std::chrono::system_clock::now().time_since_epoch().count();
    return "gateway-" + std::to_string(started) + "-" + std::to_string(++sequence);
}
HttpResult failure(const api::Error& error, const std::string& id = {}) {
    return {api::httpStatus(error.code), {{"request_id", id.empty() ? newId() : id}, {"success", false},
        {"error_code", api::errorCodeName(error.code)}, {"error", error.message}}};
}
Json safeModel(const api::ModelConfig& model) {
    return {{"id", model.id}, {"name", model.name}, {"is_mock", model.is_mock},
        {"enabled", model.enabled}, {"scores", model.scores}};
}
} // namespace

Service::Service(std::string config_path, std::string log_path)
    : config_path_(std::move(config_path)), log_path_(std::move(log_path)) {
    const auto path = std::filesystem::u8path(log_path_);
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream check(path, std::ios::app | std::ios::binary);
    if (!check) throw std::runtime_error("无法打开请求日志文件");
}

api::ModelListResult Service::snapshot() {
    return api::loadModels(config_path_);
}

void Service::log(const Json& record) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::ofstream output(std::filesystem::u8path(log_path_), std::ios::app | std::ios::binary);
    output << record.dump() << '\n';
    if (!output) std::cerr << "Warning: request log could not be written.\n";
}

HttpResult Service::models() {
    const auto config = snapshot();
    if (!config.success) return failure(config.error);
    Json entries = Json::array();
    for (const auto& model : config.models) if (model.enabled)
        entries.push_back({{"id", model.id}, {"name", model.name}, {"is_mock", model.is_mock}});
    return {200, {{"success", true}, {"models", entries}}};
}

HttpResult Service::settings() {
    const auto config = api::loadModels(config_path_);
    if (!config.success) return failure(config.error);
    Json entries = Json::array();
    for (const auto& model : config.models) entries.push_back(safeModel(model));
    return {200, {{"success", true}, {"models", entries}}};
}

HttpResult Service::saveSettings(const std::string& body) {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    auto config = api::loadModels(config_path_);
    if (!config.success) return failure(config.error);
    try {
        const auto input = Json::parse(body);
        const auto& changes = input.at("models");
        if (!changes.is_array() || changes.empty()) throw std::runtime_error("models");
        std::set<std::string> updated;
        for (const auto& change : changes) {
            const auto id = change.at("id").get<std::string>();
            if (!updated.insert(id).second) throw std::runtime_error("duplicate");
            auto model = std::find_if(config.models.begin(), config.models.end(),
                [&](const auto& value) { return value.id == id; });
            if (model == config.models.end()) return failure({api::ErrorCode::UnknownModel, "设置中的模型不存在"});
            model->enabled = change.at("enabled").get<bool>();
            const auto& scores = change.at("scores");
            if (!scores.is_object() || scores.size() != api::task_names.size()) throw std::runtime_error("scores");
            for (const auto task : api::task_names) {
                const auto& score = scores.at(std::string(task));
                if (!score.is_number_integer() || score < 0 || score > 100) throw std::runtime_error("score");
                model->scores[std::string(task)] = score.get<int>();
            }
        }
        const auto saved = api::saveModels(config_path_, config.models);
        if (!saved.success) return failure(saved.error);
        return {200, {{"success", true}, {"message", "模型设置已保存，新问题立即生效"}}};
    } catch (...) {
        return failure({api::ErrorCode::InvalidRequest, "模型设置格式不正确，评分必须是 0 到 100 的整数"});
    }
}

HttpResult Service::chat(const std::string& raw) {
    const auto started = api::RequestContext::Clock::now();
    api::RequestContext context;
    context.request_id = newId();
    api::ChatRequest request;
    api::AnalysisResult analysis;
    Json attempts = Json::array();
    bool switched = false;
    std::string selected;
    const auto finish = [&](HttpResult response) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            api::RequestContext::Clock::now() - started).count();
        log({{"request_id", context.request_id}, {"mode", request.mode},
            {"task", analysis.reason.empty() ? Json(nullptr) : Json(analysis.task)},
            {"selected_model", selected.empty() ? Json(nullptr) : Json(selected)},
            {"success", response.body.value("success", false)}, {"fallback_used", switched},
            {"error_code", response.body.value("error_code", "none")},
            {"elapsed_ms", elapsed}, {"attempts", attempts}});
        return response;
    };
    const auto fail = [&](api::ErrorCode code, const std::string& message) {
        auto response = failure({code, message}, context.request_id);
        response.body["fallback_used"] = switched;
        return finish(std::move(response));
    };
    try {
        const auto input = Json::parse(raw, nullptr, false);
        if (input.is_discarded()) return fail(api::ErrorCode::InvalidRequest, "JSON 格式有误");
        if (!input.is_object()) return fail(api::ErrorCode::InvalidRequest, "请求内容必须是 JSON 对象");
        for (const auto* field : {"mode", "model", "message"})
            if (!input.contains(field)) return fail(api::ErrorCode::InvalidRequest, std::string("缺少字段：") + field);
        if (!input["mode"].is_string() || !input["message"].is_string() ||
            !(input["model"].is_null() || input["model"].is_string()))
            return fail(api::ErrorCode::InvalidRequest, "mode、message 必须是字符串，model 必须是字符串或 null");
        request.mode = input["mode"].get<std::string>();
        request.message = trim(input["message"].get<std::string>());
        if (!input["model"].is_null()) request.model = input["model"].get<std::string>();
        if (request.mode != "auto" && request.mode != "manual") return fail(api::ErrorCode::InvalidRequest, "mode 只能是 auto 或 manual");
        if (request.mode == "auto" && request.model) return fail(api::ErrorCode::InvalidRequest, "自动模式的 model 必须为 null");
        if (request.mode == "manual" && (!request.model || request.model->empty()))
            return fail(api::ErrorCode::InvalidRequest, "手动模式必须指定模型");
        if (request.message.empty()) return fail(api::ErrorCode::InvalidRequest, "问题不能为空");
        if (input.contains("demo_failure") && !input["demo_failure"].is_boolean())
            return fail(api::ErrorCode::InvalidRequest, "故障演示开关必须是布尔值");
        if (input.value("demo_failure", false))
            return fail(api::ErrorCode::InvalidRequest, "故障模拟由 D 的测试服务提供，本版不注入模型故障");
        auto config = snapshot();
        if (!config.success) return fail(config.error.code, config.error.message);
        analysis = api::analyzeQuestion(request.message);
        const auto route = api::selectModel(analysis, request, config.models);
        if (!route.success) return fail(route.error.code, route.error.message);
        std::string next = route.primary_model;
        for (int attempt = 0; attempt < 2; ++attempt) {
            const auto model = std::find_if(config.models.begin(), config.models.end(),
                [&](const auto& value) { return value.id == next && value.enabled; });
            if (model == config.models.end()) return fail(api::ErrorCode::InvalidConfig, "选择结果未对应到可用模型");
            auto actual = *model;
            const auto result = api::callModel(actual, request, context);
            attempts.push_back({{"model_id", result.model_id}, {"http_status", result.http_status},
                {"elapsed_ms", result.elapsed_ms}, {"success", result.success},
                {"error_code", api::errorCodeName(result.error.code)}});
            if (result.success) {
                selected = result.model_id;
                auto reason = analysis.reason + "；" + route.reason;
                if (switched) reason += "；首选模型 " + route.primary_model + " 明确返回 HTTP " +
                    std::to_string(attempts.front().at("http_status").get<int>()) + "，改用备用模型 " + selected;
                return finish({200, {{"request_id", context.request_id}, {"success", true},
                    {"task", analysis.task}, {"uncertain", analysis.uncertain}, {"selected_model", selected},
                    {"is_mock", actual.is_mock}, {"reason", reason}, {"answer", result.answer},
                    {"fallback_used", switched}}});
            }
            if (route.fallback_model && *route.fallback_model != actual.id &&
                api::mayUseFallback(request, result, context, attempt + 1)) {
                next = *route.fallback_model;
                switched = true;
                continue;
            }
            return fail(result.error.code, result.error.message);
        }
        return fail(api::ErrorCode::InternalError, "调用流程未得到有效结果");
    } catch (...) {
        return fail(api::ErrorCode::InternalError, "网关处理发生内部错误");
    }
}
} // namespace app
