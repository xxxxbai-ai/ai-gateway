#include "shared/module_api.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace gateway_contract;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: router_contract_demo <models.json>\n";
        return 2;
    }
    const fs::path copy = fs::temp_directory_path() /
        ("gateway_router_contract_demo_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".json");
    try {
        fs::copy_file(argv[1], copy, fs::copy_options::overwrite_existing);
        auto loaded = loadModels(copy.string());
        if (!loaded.success) throw std::runtime_error(loaded.error.message);
        ChatRequest request;
        request.message = "计算平方根";
        AnalysisResult analysis{"math", "demo", false};
        auto show = [&](const char* label) {
            const auto result = selectModel(analysis, request, loaded.models);
            std::cout << label << ": ";
            if (result.success) {
                std::cout << result.primary_model << ", fallback="
                          << result.fallback_model.value_or("none") << ", reason="
                          << result.reason << '\n';
            } else {
                std::cout << errorCodeName(result.error.code) << ": "
                          << result.error.message << '\n';
            }
        };
        show("auto");
        request.mode = "manual";
        request.model = "mock_code";
        show("manual");
        request.model = "real_a";
        show("disabled");
        request.mode = "auto";
        request.model.reset();
        for (auto& model : loaded.models)
            if (model.id == "mock_math") model.enabled = false;
        auto saved = saveModels(copy.string(), loaded.models);
        if (!saved.success) throw std::runtime_error(saved.error.message);
        loaded = loadModels(copy.string());
        if (!loaded.success) throw std::runtime_error(loaded.error.message);
        show("after save and reload");
        std::error_code ignored;
        fs::remove(copy, ignored);
        return 0;
    } catch (const std::exception& error) {
        std::error_code ignored;
        fs::remove(copy, ignored);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
