#include "test_doubles.h"
#include <stdexcept>

namespace a_test {
namespace {
gateway_contract::ModelConfig model(const std::string& id) {
    gateway_contract::ModelConfig value;
    value.id = id;
    value.name = "仅测试：" + id;
    for (const auto task : gateway_contract::task_names) value.scores[std::string(task)] = 50;
    value.provider_model = "not-a-real-model";
    value.base_url = "http://127.0.0.1:1/test-double";
    value.credential_ref = "env:TEST_ONLY_KEY";
    return value;
}
State initial() {
    State value;
    value.models = {model("a_test_primary"), model("a_test_backup")};
    value.analysis = {"programming", "固定的分类测试结果，不执行 B 的规则", false};
    value.route = {true, "a_test_primary", std::string("a_test_backup"), "固定的选模测试结果", {}};
    return value;
}
}
State& state() { static State value = initial(); return value; }
void reset() { state() = initial(); }
} // namespace a_test

namespace gateway_contract {
AnalysisResult analyzeQuestion(const std::string&) {
    auto& fixture = a_test::state();
    ++fixture.analyses;
    if (fixture.throw_analysis) throw std::runtime_error("injected test exception");
    return fixture.analysis;
}
ModelListResult loadModels(const std::string&) {
    const auto& fixture = a_test::state();
    if (fixture.load_error.code != ErrorCode::None) return {false, {}, fixture.load_error};
    return {true, fixture.models, {}};
}
OperationResult saveModels(const std::string&, const std::vector<ModelConfig>& models) {
    auto& fixture = a_test::state();
    ++fixture.saves;
    if (fixture.save_error.code != ErrorCode::None) return {false, fixture.save_error};
    fixture.models = models; // 内存回执；不实现 C 的磁盘保存。
    return {true, {}};
}
RouteResult selectModel(const AnalysisResult& analysis, const ChatRequest& request,
                        const std::vector<ModelConfig>&) {
    auto& fixture = a_test::state();
    ++fixture.routes;
    fixture.routed_request = request;
    fixture.routed_analysis = analysis;
    return fixture.route;
}
CallResult callModel(const ModelConfig& model, const ChatRequest& request, RequestContext& context) {
    auto& fixture = a_test::state();
    const auto index = fixture.calls.size();
    fixture.calls.push_back({model.id, request.message, context.request_id, context.deadline, context.cancelled});
    CallResult result;
    if (index < fixture.outcomes.size()) result = fixture.outcomes[index];
    else { result.success = true; result.answer = "TEST DOUBLE ONLY: " + request.message; result.http_status = 200; }
    result.request_id = context.request_id;
    result.model_id = model.id;
    if (fixture.expire_after_call) context.deadline = RequestContext::Clock::now() - std::chrono::seconds(1);
    if (fixture.cancel_after_call) context.cancelled->store(true);
    return result;
}
} // namespace gateway_contract
