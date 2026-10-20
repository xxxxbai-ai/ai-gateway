#include "providers/mock_provider.h"

namespace gateway {
AttemptResult MockProvider::invoke(const ModelConfig& model, const ChatRequest& request,
                                   RequestContext& context, EventSink sink) {
    AttemptResult result;
    result.request_id = context.request_id;
    result.model_id = model.id;
    result.provider_model = model.id;
    if (context.cancelled->load()) {
        result.error_code = ProviderErrorCode::Cancelled;
        result.error_message = "请求已取消";
        return result;
    }
    if (RequestContext::Clock::now() >= context.deadline) {
        result.error_code = ProviderErrorCode::TotalTimeout;
        result.error_message = "请求总时限已到";
        return result;
    }
    result.answer = "[模拟回答] " + model.id + " 已收到：" + request.question;
    try {
        if (sink && (!sink({EventKind::Delta, result.answer}) || !sink({EventKind::Finish}))) {
            context.cancelled->store(true);
            result.error_code = ProviderErrorCode::Cancelled;
            result.error_message = "调用方停止接收";
            return result;
        }
    } catch (...) {
        result.error_code = ProviderErrorCode::CallbackFailed;
        result.error_message = "调用方事件处理失败";
        return result;
    }
    result.completed = true;
    return result;
}
}
