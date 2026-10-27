#pragma once
#include "../../shared/module_api.h"
#include <vector>

// 仅供 A 单独测试：预设输入输出，不实现 B 的规则、C 的排序或 D 的网络调用。
namespace a_test {
struct Invocation {
    std::string model, message, request_id;
    gateway_contract::RequestContext::Clock::time_point deadline;
    std::shared_ptr<std::atomic_bool> cancelled;
};
struct State {
    std::vector<gateway_contract::ModelConfig> models;
    gateway_contract::AnalysisResult analysis;
    gateway_contract::RouteResult route;
    gateway_contract::Error load_error, save_error;
    std::vector<gateway_contract::CallResult> outcomes;
    std::vector<Invocation> calls;
    gateway_contract::ChatRequest routed_request;
    gateway_contract::AnalysisResult routed_analysis;
    int analyses = 0, routes = 0, saves = 0;
    bool throw_analysis = false, expire_after_call = false, cancel_after_call = false;
};
State& state();
void reset();
} // namespace a_test
