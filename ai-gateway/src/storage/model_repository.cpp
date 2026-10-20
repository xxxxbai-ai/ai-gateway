#include "storage/model_repository.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <utility>

namespace gateway {
ModelRepository::ModelRepository() {
    // 均为虚构模型与演示分数，不代表真实厂商能力。
    models_ = {
        {"demo-code",     {5, 3, 2, 2, 3}},
        {"demo-language", {2, 2, 5, 5, 3}},
        {"demo-general",  {3, 5, 3, 3, 5}},
    };
    for (auto& model : models_) {
        model.provider_model = model.id;
        model.base_url = "http://127.0.0.1:18080/v1";
        model.simulated = true;
    }
}
ModelRepository::ModelRepository(std::vector<ModelConfig> models) : models_(std::move(models)) {}

std::vector<ModelConfig> ModelRepository::listModels() const { return models_; }

std::vector<ModelConfig> ModelRepository::loadConfig(const std::string& path) {
    using Json = nlohmann::json;
    std::ifstream file(std::filesystem::u8path(path));
    if (!file) throw std::runtime_error("无法打开模型配置文件");
    try {
        const auto document = Json::parse(file);
        const auto& entries = document.at("models");
        if (!entries.is_array() || entries.empty()) throw std::runtime_error("models 必须是非空数组");
        std::vector<ModelConfig> models;
        std::set<std::string> ids;
        for (const auto& entry : entries) {
            ModelConfig model;
            model.id = entry.at("id").get<std::string>();
            if (model.id.empty() || model.id == "auto" || !ids.insert(model.id).second)
                throw std::runtime_error("模型 ID 不能为空、重复或使用保留名 auto");
            model.base_url = entry.at("base_url").get<std::string>();
            model.provider_model = entry.value("provider_model", model.id);
            model.credential_ref = entry.value("credential_ref", "");
            model.protocol = entry.value("protocol", "chat_completions");
            model.enabled = entry.value("enabled", true);
            model.simulated = entry.value("simulated", false);
            if (entry.contains("abilities")) {
                const auto& scores = entry.at("abilities");
                if (!scores.is_array() || scores.size() != model.abilities.size())
                    throw std::runtime_error("abilities 需要依次填写编程、数学、写作、翻译、通用的五项评分");
                for (std::size_t i = 0; i < scores.size(); ++i) {
                    if (!scores[i].is_number_integer() || scores[i] < 0 || scores[i] > 5)
                        throw std::runtime_error("演示能力评分必须是 0 到 5 的整数");
                    model.abilities[i] = scores[i].get<int>();
                }
            }
            models.push_back(std::move(model));
        }
        return models;
    } catch (const Json::exception&) {
        // 解析异常可能带有原文，不能直接打印含凭证的错误片段。
        throw std::runtime_error("模型配置不是有效的 UTF-8 JSON，或字段类型不正确");
    }
}
}
