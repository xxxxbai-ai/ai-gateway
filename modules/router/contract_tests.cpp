#include "shared/module_api.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace gateway_contract;
namespace fs = std::filesystem;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    check(argc == 2, "missing example config path");
    auto loaded = loadModels(argv[1]);
    check(loaded.success && loaded.models.size() == 4, "example config load failed");
    ChatRequest request;
    request.message = "计算平方根";
    AnalysisResult analysis{"math", "test", false};
    auto route = selectModel(analysis, request, loaded.models);
    check(route.success && route.primary_model == "mock_math" &&
          route.fallback_model == "mock_code", "automatic route failed");
    request.mode = "manual";
    request.model = "mock_code";
    route = selectModel(analysis, request, loaded.models);
    check(route.success && route.primary_model == "mock_code" && !route.fallback_model,
          "manual route failed");
    request.model = "real_a";
    check(selectModel(analysis, request, loaded.models).error.code == ErrorCode::DisabledModel,
          "disabled model accepted");
    request.model = "missing";
    check(selectModel(analysis, request, loaded.models).error.code == ErrorCode::UnknownModel,
          "unknown model accepted");
    request.mode = "auto";
    check(selectModel(analysis, request, loaded.models).error.code == ErrorCode::InvalidRequest,
          "invalid auto model accepted");
    request.model.reset();
    auto reordered = loaded.models;
    std::reverse(reordered.begin(), reordered.end());
    check(selectModel(analysis, request, reordered).primary_model == "mock_math",
          "input order changed route");
    reordered[0].id = reordered[1].id;
    check(selectModel(analysis, request, reordered).error.code == ErrorCode::InvalidConfig,
          "duplicate id accepted");
    auto disabled = loaded.models;
    for (auto& model : disabled) model.enabled = false;
    check(selectModel(analysis, request, disabled).error.code == ErrorCode::NoAvailableModel,
          "all-disabled config accepted");
    const fs::path saved = fs::absolute(argv[0]).parent_path() / "gateway_router_contract_test.json";
    const auto saved_result = saveModels(saved.string(), loaded.models);
    if (!saved_result.success) throw std::runtime_error("save failed: " + saved_result.error.message);
    auto restored = loadModels(saved.string());
    check(restored.success && restored.models.size() == loaded.models.size(), "reload failed");
    auto invalid = loaded.models;
    invalid[0].scores.erase("math");
    check(!saveModels(saved.string(), invalid).success, "invalid config saved");
    check(loadModels(saved.string()).success, "failed save damaged existing config");
    restored.models[1].enabled = false;
    check(saveModels(saved.string(), restored.models).success, "replacement save failed");
    restored = loadModels(saved.string());
    check(restored.success && !restored.models[1].enabled, "replacement did not persist");
    route = selectModel(analysis, request, restored.models);
    check(route.success && route.primary_model == "mock_code", "new config not applied");
    std::error_code ignored;
    fs::remove(saved, ignored);
    std::cout << "Contract routing checks passed.\n";
}
