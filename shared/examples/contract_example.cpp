#include "module_api.h"
#include "../../modules/analyzer/cli_support.h"

#include <iostream>

int main(int argc, char** argv) {
    analyzer_cli::arguments(argc, argv);
    gateway_contract::ChatRequest request;
    request.message = "翻译这段 C++ 报错";
    const auto analysis = gateway_contract::analyzeQuestion(request.message);

    gateway_contract::ModelConfig model;
    model.id = "mock";
    model.name = "模拟通用模型";
    model.scores = {{"programming", 65}, {"math", 65}, {"translation", 90},
                    {"writing", 90}, {"general", 90}};
    model.provider_model = "demo-language";
    model.base_url = "http://127.0.0.1:18080/v1";

    std::cout << "公共接口版本: " << gateway_contract::version << '\n'
              << "B 实际分类: " << analysis.task << '\n'
              << "B 实际理由: " << analysis.reason << '\n'
              << "模型配置示例: " << model.id << " -> " << model.provider_model << '\n'
              << "此程序验证公共类型与 B 调用；C/D 的新接口仍需各负责人适配。\n";
    return analysis.task == "translation" && !analysis.uncertain ? 0 : 1;
}
