#include "mock_server.h"
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    int port = 18080, delay = 200;
    try {
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help") {
                std::cout << "mock_provider [--port 18080] [--delay-ms 200]\n"
                    << "三个模拟模型：demo-code、demo-language、demo-general\n"
                    << "接口：/health、/v1/models、/v1/chat/completions、/stats\n"
                    << "故障地址：/scenario/<场景>/v1/chat/completions\n"
                    << "场景：normal、fragmented、packed、no_usage、reject429、reject503、\n"
                    << "unauthorized、delay、disconnect、stall、heartbeats、bad_json、\n"
                    << "stream_error、missing_done。Ctrl+C 退出。\n";
                return 0;
            }
            if (i + 1 >= argc) throw std::runtime_error("参数缺少值");
            if (argument == "--port") port = std::stoi(argv[++i]);
            else if (argument == "--delay-ms") delay = std::stoi(argv[++i]);
            else throw std::runtime_error("未知参数，使用 --help 查看用法");
        }
        if (port < 1 || port > 65535 || delay < 0 || delay > 10000)
            throw std::runtime_error("端口或延迟超出范围");
        gateway::MockHttpServer server(delay);
        if (server.bind(port) < 0) throw std::runtime_error("端口已被占用，使用 --port 更换");
        std::cout << "模拟服务：http://127.0.0.1:" << port << "/v1\n"
                  << "统计：http://127.0.0.1:" << port << "/stats\n" << std::flush;
        return server.listen() ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "错误：" << error.what() << '\n';
        return 1;
    }
}
