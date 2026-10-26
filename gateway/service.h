#pragma once
#include "../shared/module_api.h"
#include <nlohmann/json.hpp>
#include <mutex>

namespace app {
struct HttpResult {
    int status;
    nlohmann::json body;
};

class Service {
public:
    Service(std::string config_path, std::string log_path, int managed_mock_port = 0);
    HttpResult chat(const std::string& body);
    HttpResult models();
    HttpResult settings();
    HttpResult saveSettings(const std::string& body);
private:
    gateway_contract::ModelListResult snapshot();
    std::string config_path_, log_path_;
    int mock_port_;
    std::mutex log_mutex_, settings_mutex_;
    void log(const nlohmann::json& record);
};
} // namespace app
