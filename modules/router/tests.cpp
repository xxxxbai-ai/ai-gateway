#include "model_router.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

int main() {
    auto models = routing::initialModels();
    const std::vector<std::string> expected{
        "mock_code", "mock_math", "mock", "mock", "mock"};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto result = routing::selectModel(routing::taskTypes[i], models);
        check(result.success && result.selected_model == expected[i] &&
                  !result.reason.empty(), "basic selection failed");
    }
    check(!routing::selectModel("unknown", models).success, "unknown task accepted");
    check(!routing::selectModel("math", {}).success, "empty models accepted");

    for (auto& model : models) model.scores["math"] = 80;
    check(routing::selectModel("math", models).selected_model == "mock", "tie failed");
    std::reverse(models.begin(), models.end());
    check(routing::selectModel("math", models).selected_model == "mock", "order affected tie");
    for (auto& model : models) model.enabled = false;
    check(!routing::selectModel("math", models).success, "disabled models selected");

    models = routing::initialModels();
    models[1].scores.erase("math");
    models[0].scores["math"] = 101;
    check(routing::selectModel("math", models).selected_model == "mock", "invalid score selected");
    models.push_back(models.front());
    check(!routing::selectModel("math", models).success, "duplicate id accepted");
    models = routing::initialModels();
    models[0].id.clear();
    check(!routing::selectModel("math", models).success, "empty id accepted");
    std::cout << "All routing checks passed.\n";
}
