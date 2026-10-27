#pragma once

#include "contracts.h"

namespace gateway_contract {

// B：空白/无法判断返回 general + uncertain，原因写入 reason。
AnalysisResult analyzeQuestion(const std::string& message);

// C：配置读取失败返回结构化错误，不退回一份隐式默认配置。
ModelListResult loadModels(const std::string& config_path);

// C：保存成功后新请求重新读取；失败不得破坏原有配置。
OperationResult saveModels(const std::string& config_path,
                           const std::vector<ModelConfig>& models);

// C：自动选择首选及至多一个不同备用；手动只返回指定模型，不提供备用。
RouteResult selectModel(const AnalysisResult& analysis, const ChatRequest& request,
                        const std::vector<ModelConfig>& models);

// D：只调用这一个模型一次；按传入 deadline/取消状态工作，不选模、不跨模型重试。
CallResult callModel(const ModelConfig& model, const ChatRequest& request,
                     RequestContext& context);

} // namespace gateway_contract
