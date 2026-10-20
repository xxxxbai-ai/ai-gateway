#include "model_router.h"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const auto models = routing::initialModels();
    const std::vector<std::string> tasks =
        argc > 1 ? std::vector<std::string>(argv + 1, argv + argc)
                 : routing::taskTypes;
    bool success = true;
    for (const auto& task : tasks) {
        const auto result = routing::selectModel(task, models);
        if (result.success) {
            std::cout << task << " -> " << result.selected_model << '\n'
                      << result.reason << '\n';
        } else {
            std::cerr << result.error << '\n';
            success = false;
        }
    }
    return success ? 0 : 1;
}
