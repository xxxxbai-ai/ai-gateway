#include "module_api.h"
#include "../../modules/analyzer/question_analyzer.h"
#include "../../modules/analyzer/cli_support.h"

#include <iostream>
#include <stdexcept>
#include <type_traits>

namespace api = gateway_contract;

// 这些断言只检查 C/D 的声明可供独立调用方编译，不伪造其实现或验收结果。
static_assert(std::is_same_v<decltype(&api::loadModels),
    api::ModelListResult (*)(const std::string&)>);
static_assert(std::is_same_v<decltype(&api::saveModels),
    api::OperationResult (*)(const std::string&, const std::vector<api::ModelConfig>&)>);
static_assert(std::is_same_v<decltype(&api::selectModel),
    api::RouteResult (*)(const api::AnalysisResult&, const api::ChatRequest&,
                        const std::vector<api::ModelConfig>&)>);
static_assert(std::is_same_v<decltype(&api::callModel),
    api::CallResult (*)(const api::ModelConfig&, const api::ChatRequest&, api::RequestContext&)>);
static_assert(std::is_same_v<classification::AnalysisResult, api::AnalysisResult>);

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
} // namespace

int main(int argc, char** argv) {
    try {
        analyzer_cli::arguments(argc, argv);
        require(api::version == "1.0", "公共接口版本错误");
        require(api::isTaskName("programming") && api::isTaskName("general") &&
                !api::isTaskName("code") && !api::isTaskName("编程"), "任务名称不统一");

        api::ChatRequest request;
        request.message = "翻译这段 C++ 报错";
        require(request.mode == "auto" && !request.model, "自动模式不能携带指定模型");
        const api::AnalysisResult analysis = api::analyzeQuestion(request.message);
        require(analysis.task == "translation" && !analysis.uncertain && !analysis.reason.empty(),
                "B 未通过公共接口返回实际结果");
        const auto compatible = classification::analyzeQuestion(request.message);
        require(compatible.task == analysis.task && compatible.reason == analysis.reason,
                "B 旧入口与公共入口不兼容");
        const auto unknown = api::analyzeQuestion("你好");
        require(unknown.task == "general" && unknown.uncertain, "不确定分类必须回退 general");

        require(api::httpStatus(api::ErrorCode::UnknownModel) == 400, "未知模型应返回 400");
        require(api::httpStatus(api::ErrorCode::DisabledModel) == 400, "禁用模型应返回 400");
        require(api::httpStatus(api::ErrorCode::InvalidConfig) == 500, "配置错误应返回 500");
        require(api::httpStatus(api::ErrorCode::InvalidResponse) == 502, "无效上游响应应返回 502");
        require(api::httpStatus(api::ErrorCode::MissingCredential) == 503, "网关缺少凭证应返回 503");
        require(api::httpStatus(api::ErrorCode::TotalTimeout) == 504, "超时应返回 504");
        require(std::string(api::errorCodeName(api::ErrorCode::NoAvailableModel)) == "no_available_model",
                "客户端错误标识不稳定");

        api::RequestContext context;
        context.request_id = "contract-check";
        api::CallResult failure;
        failure.request_id = context.request_id;
        failure.model_id = "mock_code";
        failure.error = {api::ErrorCode::UpstreamRejected, "上游明确拒绝"};
        failure.explicit_rejection = true;
        failure.http_status = 503;
        require(api::mayUseFallback(request, failure, context, 1), "首次明确 503 应允许备用");
        failure.http_status = 429;
        require(api::mayUseFallback(request, failure, context, 1), "首次明确 429 应允许备用");
        require(!api::mayUseFallback(request, failure, context, 2), "不得尝试第三个模型");
        require(!api::mayUseFallback(request, failure, context, 0), "调用前不能使用失败切换");
        require(!api::mayUseFallback(request, failure, context, 1, true), "开始输出后不能切换");
        request.mode = "manual";
        request.model = "mock_code";
        require(!api::mayUseFallback(request, failure, context, 1), "手动模式不能切换");
        request.mode = "auto";
        require(!api::mayUseFallback(request, failure, context, 1), "非法自动请求不能触发备用");
        request.model.reset();
        failure.http_status = 401;
        require(!api::mayUseFallback(request, failure, context, 1), "鉴权失败不能切换");
        failure.http_status = 503;
        failure.explicit_rejection = false;
        require(!api::mayUseFallback(request, failure, context, 1), "非明确拒绝不能切换");
        failure.explicit_rejection = true;
        failure.success = true;
        require(!api::mayUseFallback(request, failure, context, 1), "成功调用不能切换");
        failure.success = false;
        failure.error.code = api::ErrorCode::TotalTimeout;
        require(!api::mayUseFallback(request, failure, context, 1), "超时不能误作明确拒绝");
        failure.error.code = api::ErrorCode::UpstreamRejected;
        context.cancelled->store(true);
        require(!api::mayUseFallback(request, failure, context, 1), "取消后不能切换");
        context.cancelled->store(false);
        context.deadline = api::RequestContext::Clock::now() - std::chrono::seconds(1);
        require(!api::mayUseFallback(request, failure, context, 1), "过期后不能切换");
        context.deadline = api::RequestContext::Clock::now() + std::chrono::seconds(1);
        context.cancelled.reset();
        require(!api::mayUseFallback(request, failure, context, 1), "缺失取消状态时保守禁止备用");

        std::cout << "公共头文件、C/D 声明、B 实际调用、错误映射和备用边界: PASS\n";
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
    return 0;
}
