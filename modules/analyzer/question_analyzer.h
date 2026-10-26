#pragma once

#include "../../shared/module_api.h"

namespace classification {

// 保留 B 原有调用入口；实际类型与函数来自公共接口 1.0，不再另定义结构。
using AnalysisResult = gateway_contract::AnalysisResult;
using gateway_contract::analyzeQuestion;

} // namespace classification
