#include "shared/module_api.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace gateway_contract {
namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;

Error error(ErrorCode code, std::string message) { return {code, std::move(message)}; }

std::string validate(const std::vector<ModelConfig>& models) {
    std::set<std::string> ids;
    for (const auto& m : models) {
        if (m.id.empty() || m.id == "auto" || !ids.insert(m.id).second)
            return "模型编号为空、重复或使用保留值 auto";
        if (m.name.empty() || m.provider_model.empty() || m.base_url.empty() ||
            m.protocol != "chat_completions") return "模型 " + m.id + " 的调用配置无效";
        if (m.base_url.rfind("https://", 0) != 0 &&
            !(m.is_mock && (m.base_url.rfind("http://127.0.0.1/", 0) == 0 ||
                            m.base_url.rfind("http://127.0.0.1:", 0) == 0 ||
                            m.base_url.rfind("http://localhost/", 0) == 0 ||
                            m.base_url.rfind("http://localhost:", 0) == 0)))
            return "模型 " + m.id + " 的地址必须使用 HTTPS；本机模拟模型可使用 HTTP";
        if (!m.is_mock && (m.credential_ref.rfind("env:", 0) != 0 ||
            m.credential_ref.size() <= 4)) return "真实模型 " + m.id + " 缺少凭证引用";
        if (!m.credential_ref.empty() && (m.credential_ref.rfind("env:", 0) != 0 ||
            m.credential_ref.size() <= 4)) return "模型 " + m.id + " 的凭证引用无效";
        if (m.scores.size() != task_names.size()) return "模型 " + m.id + " 的五类评分不完整";
        for (const auto task : task_names) {
            const auto score = m.scores.find(std::string(task));
            if (score == m.scores.end() || score->second < 0 || score->second > 100)
                return "模型 " + m.id + " 的评分无效";
        }
    }
    return {};
}

json encode(const ModelConfig& m) {
    return {{"id", m.id}, {"name", m.name}, {"enabled", m.enabled},
            {"is_mock", m.is_mock}, {"scores", m.scores},
            {"provider_model", m.provider_model}, {"base_url", m.base_url},
            {"credential_ref", m.credential_ref}, {"protocol", m.protocol}};
}

ModelConfig decode(const json& value) {
    if (!value.is_object()) throw std::runtime_error("模型项必须是对象");
    ModelConfig m;
    m.id = value.at("id").get<std::string>();
    m.name = value.at("name").get<std::string>();
    m.enabled = value.at("enabled").get<bool>();
    m.is_mock = value.at("is_mock").get<bool>();
    m.scores = value.at("scores").get<std::map<std::string, int>>();
    m.provider_model = value.at("provider_model").get<std::string>();
    m.base_url = value.at("base_url").get<std::string>();
    m.credential_ref = value.at("credential_ref").get<std::string>();
    m.protocol = value.at("protocol").get<std::string>();
    return m;
}
}

ModelListResult loadModels(const std::string& config_path) {
    try {
        std::ifstream input(config_path, std::ios::binary);
        if (!input) return {false, {}, error(ErrorCode::InvalidConfig, "无法读取模型配置文件")};
        const auto document = json::parse(input);
        if (!document.is_object() || document.at("contract_version").get<std::string>() != version ||
            !document.at("models").is_array())
            throw std::runtime_error("配置版本或 models 字段无效");
        std::vector<ModelConfig> models;
        for (const auto& item : document.at("models")) models.push_back(decode(item));
        if (const auto problem = validate(models); !problem.empty())
            return {false, {}, error(ErrorCode::InvalidConfig, problem)};
        return {true, std::move(models), {}};
    } catch (const std::exception& ex) {
        return {false, {}, error(ErrorCode::InvalidConfig, std::string("模型配置无效：") + ex.what())};
    }
}

OperationResult saveModels(const std::string& config_path, const std::vector<ModelConfig>& models) {
    if (const auto problem = validate(models); !problem.empty())
        return {false, error(ErrorCode::InvalidConfig, problem)};
    static std::atomic<unsigned long long> sequence{0};
    const fs::path target(config_path);
    const fs::path temporary = target.string() + ".tmp." +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." +
        std::to_string(++sequence);
    try {
        json document = {{"contract_version", "1.0"}, {"models", json::array()}};
        for (const auto& model : models) document["models"].push_back(encode(model));
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("无法创建临时配置文件");
            output << document.dump(2) << '\n';
            output.flush();
            if (!output) throw std::runtime_error("写入模型配置失败");
        }
#ifdef _WIN32
        const auto dest = target.wstring(), source = temporary.wstring();
        if (fs::exists(target)) {
            if (!ReplaceFileW(dest.c_str(), source.c_str(), nullptr, 0, nullptr, nullptr))
                throw std::runtime_error("替换模型配置失败");
        } else if (!MoveFileExW(source.c_str(), dest.c_str(), MOVEFILE_WRITE_THROUGH)) {
            throw std::runtime_error("保存模型配置失败");
        }
#else
        fs::rename(temporary, target);
#endif
        return {true, {}};
    } catch (const std::exception& ex) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        return {false, error(ErrorCode::InvalidConfig, ex.what())};
    }
}

RouteResult selectModel(const AnalysisResult& analysis, const ChatRequest& request,
                        const std::vector<ModelConfig>& models) {
    const auto fail = [](ErrorCode code, std::string message) -> RouteResult {
        return {false, {}, std::nullopt, {}, error(code, std::move(message))};
    };
    if (!isTaskName(analysis.task) || (analysis.uncertain && analysis.task != "general"))
        return fail(ErrorCode::InvalidRequest, "问题类别无效");
    if ((request.mode != "auto" && request.mode != "manual") ||
        (request.mode == "auto" && request.model) ||
        (request.mode == "manual" && (!request.model || request.model->empty())))
        return fail(ErrorCode::InvalidRequest, "选模模式或模型编号无效");
    if (const auto problem = validate(models); !problem.empty())
        return fail(ErrorCode::InvalidConfig, problem);
    if (request.mode == "manual") {
        const auto found = std::find_if(models.begin(), models.end(), [&](const ModelConfig& m) {
            return m.id == *request.model;
        });
        if (found == models.end()) return fail(ErrorCode::UnknownModel, "指定的模型不存在");
        if (!found->enabled) return fail(ErrorCode::DisabledModel, "指定的模型已禁用");
        return {true, found->id, std::nullopt, "手动指定模型 " + found->id, {}};
    }
    std::vector<const ModelConfig*> eligible;
    for (const auto& model : models) if (model.enabled) eligible.push_back(&model);
    if (eligible.empty()) return fail(ErrorCode::NoAvailableModel, "当前任务没有可用模型");
    std::sort(eligible.begin(), eligible.end(), [&](const ModelConfig* a, const ModelConfig* b) {
        const int left = a->scores.at(analysis.task), right = b->scores.at(analysis.task);
        return left != right ? left > right : a->id < b->id;
    });
    const auto* first = eligible.front();
    const auto* second = eligible.size() > 1 ? eligible[1] : nullptr;
    return {true, first->id, second ? std::optional<std::string>(second->id) : std::nullopt,
            "任务 " + analysis.task + " 选择 " + first->id + "，评分 " +
                std::to_string(first->scores.at(analysis.task)) + "；同分按编号排序", {}};
}
} // namespace gateway_contract
