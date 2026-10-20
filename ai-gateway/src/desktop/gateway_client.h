#pragma once
#include "common/contracts.h"
#include <functional>

namespace gateway {
// E负责：未来Qt页面通过此接口异步访问网关，不直接访问SQLite。
// 目前仅约定接口，不实现Qt界面；错误回调与流式增量后续补齐。
class GatewayClient {
public:
    using OnReply = std::function<void(const ChatResponse&)>;
    virtual ~GatewayClient() = default;
    virtual void send(const ChatRequest& request, OnReply onReply) = 0;
};
}
