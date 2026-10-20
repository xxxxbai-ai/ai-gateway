#pragma once
#include <memory>

namespace gateway {
// 独立 HTTP 模拟服务和测试共用实现；三个模型均无真实推理能力。
class MockHttpServer {
public:
    explicit MockHttpServer(int segment_delay_ms = 200);
    ~MockHttpServer();
    int bind(int port = 0); // 0 自动选择空闲端口；只监听本机。
    bool listen();
    void waitUntilReady() const;
    void stop();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
