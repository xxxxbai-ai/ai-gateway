#include <httplib.h>
#include "service.h"
#include "cli_arguments.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    try {
#ifdef A_GATEWAY_TEST_HOST
        std::cerr << "A GATEWAY TEST HOST: fixed B/C/D test doubles; not team integration.\n";
#endif
        const auto args = app::arguments(argc, argv);
        const auto app_dir = std::filesystem::absolute(std::filesystem::u8path(args[0])).parent_path();
        int port = 8080;
        std::string config = (app_dir / "config/models.json").u8string();
        std::string log = (app_dir / "logs/requests.jsonl").u8string();
        for (std::size_t index = 1; index < args.size(); ++index) {
            const auto& key = args[index];
            if (key == "--help") {
                std::cout << "gateway [--port 0..65535] [--config file] [--log file]\n";
                return 0;
            }
            if (index + 1 >= args.size()) throw std::runtime_error("启动参数缺少值");
            const auto& value = args[++index];
            if (key == "--config") config = value;
            else if (key == "--log") log = value;
            else if (key == "--port") {
                std::size_t count = 0;
                port = std::stoi(value, &count);
                if (count != value.size() || port < 0 || port > 65535) throw std::runtime_error("端口不正确");
            } else throw std::runtime_error("未知启动参数");
        }
        const auto configured = gateway_contract::loadModels(config);
        if (!configured.success) throw std::runtime_error(configured.error.message);

        app::Service service(config, log);
        httplib::Server server;
        server.new_task_queue = [] { return new httplib::ThreadPool(4); };
        server.set_payload_max_length(1024 * 1024);
        const auto reply = [](httplib::Response& response, const app::HttpResult& result) {
            response.status = result.status;
            response.set_content(result.body.dump(), "application/json; charset=utf-8");
        };
        server.Get("/health", [](const auto&, auto& response) {
            response.set_content("{\"success\":true,\"status\":\"ok\"}", "application/json; charset=utf-8");
        });
        server.Get("/api/models", [&](const auto&, auto& response) { reply(response, service.models()); });
        server.Get("/api/config", [&](const auto&, auto& response) { reply(response, service.settings()); });
        server.Post("/api/config", [&](const auto& request, auto& response) { reply(response, service.saveSettings(request.body)); });
        server.Post("/api/chat", [&](const auto& request, auto& response) { reply(response, service.chat(request.body)); });
        if (port == 0) port = server.bind_to_any_port("127.0.0.1");
        else if (!server.bind_to_port("127.0.0.1", port)) port = -1;
        if (port <= 0) throw std::runtime_error("端口已被占用，请换一个端口启动");
        std::cout << "Gateway ready: http://127.0.0.1:" << port << std::endl;
        return server.listen_after_bind() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Gateway error: " << error.what() << std::endl;
        return 1;
    }
}
