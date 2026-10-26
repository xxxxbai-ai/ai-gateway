#include "../../shared/module_api.h"
#include "model_router.h"
#include <nlohmann/json.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <set>
#include <system_error>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace gateway_contract {
namespace {
using Json = nlohmann::json;
std::mutex config_mutex;

std::string validate(const ModelConfig& model) {
    if (model.id.empty() || model.id == "auto" || model.name.empty()) return "模型编号和名称不能为空，编号不能为 auto";
    if (model.scores.size() != task_names.size()) return "每个模型需要五类能力评分";
    for (const auto task : task_names) {
        const auto score = model.scores.find(std::string(task));
        if (score == model.scores.end() || score->second < 0 || score->second > 100)
            return "能力评分必须是 0 到 100 的整数";
    }
    if (model.protocol != "chat_completions" || model.provider_model.empty()) return "模型协议或上游模型名称无效";
    if (model.base_url.find_first_of("\r\n\t ") != std::string::npos ||
        (model.base_url.rfind("http://", 0) != 0 && model.base_url.rfind("https://", 0) != 0))
        return "模型服务地址无效";
    if (!model.is_mock && model.base_url.rfind("https://", 0) != 0) return "真实模型必须使用 HTTPS";
    static const std::regex credential(R"(^env:[A-Za-z_][A-Za-z0-9_]*$)");
    if ((!model.credential_ref.empty() && !std::regex_match(model.credential_ref, credential)) ||
        (!model.is_mock && model.credential_ref.empty())) return "真实模型需要 env:环境变量名形式的凭证引用";
    return {};
}

std::string validateAll(const std::vector<ModelConfig>& models) {
    if (models.empty()) return "模型配置不能为空";
    std::set<std::string> ids;
    for (const auto& model : models) {
        if (!ids.insert(model.id).second) return "模型编号不能重复";
        const auto error = validate(model);
        if (!error.empty()) return error;
    }
    return {};
}

Json encode(const ModelConfig& model) {
    return {{"id", model.id}, {"name", model.name}, {"enabled", model.enabled},
        {"is_mock", model.is_mock}, {"scores", model.scores}, {"provider_model", model.provider_model},
        {"base_url", model.base_url}, {"credential_ref", model.credential_ref}, {"protocol", model.protocol}};
}
} // namespace

ModelListResult loadModels(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(config_mutex);
    try {
        std::ifstream input(std::filesystem::u8path(config_path), std::ios::binary);
        if (!input) return {false, {}, {ErrorCode::InvalidConfig, "无法打开模型配置文件"}};
        const auto document = Json::parse(input);
        if (document.at("contract_version") != "1.0" || !document.at("models").is_array())
            return {false, {}, {ErrorCode::InvalidConfig, "模型配置版本或格式不正确"}};
        std::vector<ModelConfig> models;
        for (const auto& value : document.at("models")) {
            ModelConfig model;
            model.id = value.at("id").get<std::string>();
            model.name = value.at("name").get<std::string>();
            model.enabled = value.at("enabled").get<bool>();
            model.is_mock = value.at("is_mock").get<bool>();
            const auto& scores = value.at("scores");
            if (!scores.is_object()) throw std::runtime_error("scores");
            for (const auto& score : scores.items()) {
                if (!score.value().is_number_integer() || score.value() < 0 || score.value() > 100)
                    throw std::runtime_error("integer score in range 0..100");
                model.scores[score.key()] = score.value().get<int>();
            }
            model.provider_model = value.at("provider_model").get<std::string>();
            model.base_url = value.at("base_url").get<std::string>();
            model.credential_ref = value.at("credential_ref").get<std::string>();
            model.protocol = value.at("protocol").get<std::string>();
            models.push_back(std::move(model));
        }
        const auto error = validateAll(models);
        if (!error.empty()) return {false, {}, {ErrorCode::InvalidConfig, error}};
        return {true, std::move(models), {}};
    } catch (...) {
        // JSON 错误可能包含原文，统一转换，避免输出任何凭证内容。
        return {false, {}, {ErrorCode::InvalidConfig, "模型配置不是有效的 UTF-8 JSON，或字段缺失、类型不正确"}};
    }
}

OperationResult saveModels(const std::string& config_path, const std::vector<ModelConfig>& models) {
    std::lock_guard<std::mutex> lock(config_mutex);
    const auto error = validateAll(models);
    if (!error.empty()) return {false, {ErrorCode::InvalidConfig, error}};
    auto temporary = std::filesystem::u8path(config_path);
    static std::atomic<unsigned long long> sequence{0};
    temporary += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
        "." + std::to_string(++sequence);
    try {
        Json values = Json::array();
        for (const auto& model : models) values.push_back(encode(model));
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return {false, {ErrorCode::InvalidConfig, "无法写入配置，请检查文件夹权限"}};
            output << Json{{"contract_version", "1.0"}, {"models", values}}.dump(2) << '\n';
            output.close();
            if (!output) throw std::runtime_error("write failed");
        }
        const auto target = std::filesystem::u8path(config_path);
#ifdef _WIN32
        // 同步目录或杀毒软件可能短暂占用文件；仍然只做原子替换，不先删除原配置。
        bool replaced = false;
        for (int attempt = 0; attempt < 6; ++attempt) {
            if (MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                replaced = true;
                break;
            }
            const auto error = GetLastError();
            if (error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION && error != ERROR_ACCESS_DENIED)
                break;
            if (attempt < 5) std::this_thread::sleep_for(std::chrono::milliseconds(20 << attempt));
        }
        if (!replaced) throw std::runtime_error("replace failed");
#else
        std::filesystem::rename(temporary, target);
#endif
        return {true, {}};
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return {false, {ErrorCode::InvalidConfig, "配置保存失败，原文件已保留"}};
    }
}

RouteResult selectModel(const AnalysisResult& analysis, const ChatRequest& request,
                        const std::vector<ModelConfig>& models) {
    const auto fail = [](ErrorCode code, const std::string& error) {
        return RouteResult{false, "", {}, "", {code, error}};
    };
    if (!isTaskName(analysis.task) || (request.mode != "auto" && request.mode != "manual") ||
        (request.mode == "auto" && request.model) ||
        (request.mode == "manual" && (!request.model || request.model->empty())))
        return fail(ErrorCode::InvalidRequest, "分类结果、模式或指定模型不正确");
    std::set<std::string> ids;
    for (const auto& model : models)
        if (model.id.empty() || !ids.insert(model.id).second)
            return fail(ErrorCode::InvalidConfig, "模型编号不能为空或重复");
    if (request.mode == "manual") {
        for (const auto& model : models) if (model.id == *request.model) {
            if (!model.enabled) return fail(ErrorCode::DisabledModel, "指定的模型已禁用");
            const auto error = validate(model);
            if (!error.empty()) return fail(ErrorCode::InvalidConfig, error);
            return {true, model.id, {}, "用户手动指定“" + model.name + "”，失败时不自动换模型", {}};
        }
        return fail(ErrorCode::UnknownModel, "指定的模型不存在");
    }
    std::vector<routing::ModelConfig> candidates;
    for (const auto& model : models) if (model.enabled && validate(model).empty())
        candidates.push_back({model.id, model.name, model.enabled, model.is_mock, model.scores});
    // 复用 C 已有的评分、过滤及同分排序规则，第二次在剩余候选中选备用。
    const auto primary = routing::selectModel(analysis.task, candidates);
    if (!primary.success) return fail(ErrorCode::NoAvailableModel, "当前没有可用模型，请在模型设置中启用至少一个模型");
    for (auto& model : candidates) if (model.id == primary.selected_model) model.enabled = false;
    const auto backup = routing::selectModel(analysis.task, candidates);
    RouteResult result{true, primary.selected_model, {}, primary.reason, {}};
    if (backup.success) result.fallback_model = backup.selected_model;
    return result;
}
} // namespace gateway_contract
