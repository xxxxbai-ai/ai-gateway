#include "providers/mock_provider.h"
#include "providers/compatible_chat_adapter.h"
#include "common/console.h"
#include "server/gateway_service.h"
#include <iostream>
#include <memory>
#include <stdexcept>

int main(int argc, char* argv[]) {
    gateway::enableUtf8Console();
    try {
        const auto arguments = gateway::utf8Arguments(argc, argv);
        bool http = false, stream = false, interactive = false;
        std::string config = "config/providers.example.json", message, model = "auto";
        bool single_message = false;
        for (std::size_t i = 1; i < arguments.size(); ++i) {
            const auto& arg = arguments[i];
            if (arg == "--help") {
                std::cout << "gateway_demo [--http] [--stream] [--interactive]\n"
                    << "  --http          经 B 分析、C 选模，再通过 D 调用独立 HTTP 模拟服务\n"
                    << "  --config 文件   网络模式配置，默认 config/providers.example.json\n"
                    << "  --message 文本  只发送一个问题，默认演示三类问题\n"
                    << "  --model 模型ID  默认 auto；指定模型时绝不自动切换\n"
                    << "  --stream        逐段显示回答，需配合 --http\n"
                    << "故障演示：--http --stream --config config/providers.fallback.example.json\n"
                    << "先在另一个终端运行 mock_provider；不加 --http 时保留原来的内存演示。\n";
                return 0;
            }
            if (arg == "--http") { http = true; continue; }
            if (arg == "--stream") { stream = true; continue; }
            if (arg == "--interactive") { interactive = true; continue; }
            if (i + 1 >= arguments.size()) throw std::runtime_error("参数缺少值");
            const auto& value = arguments[++i];
            if (arg == "--config") config = value;
            else if (arg == "--message") { message = value; single_message = true; }
            else if (arg == "--model") model = value;
            else throw std::runtime_error("未知参数，使用 --help 查看用法");
        }
        if (stream && !http) throw std::runtime_error("--stream 需要配合 --http");
        std::unique_ptr<gateway::ProviderAdapter> provider;
        if (http) provider = std::make_unique<gateway::CompatibleChatAdapter>();
        else provider = std::make_unique<gateway::MockProvider>();
        auto models = http ? gateway::ModelRepository::loadConfig(config) : gateway::ModelRepository().listModels();
        gateway::GatewayService service(*provider, std::move(models));
        const auto show = [&](const std::string& question) {
            gateway::ChatRequest request{question, model};
            request.stream = stream;
            gateway::RequestContext context;
            std::cout << "问题：" << question << '\n';
            if (stream) std::cout << "回答：" << std::flush;
            gateway::EventSink sink;
            if (stream) sink = [](const gateway::ProviderEvent& event) {
                if (event.kind == gateway::EventKind::Delta) std::cout << event.text << std::flush;
                return true;
            };
            const auto result = service.handle(request, context, sink);
            const auto& actual = result.attempts.back();
            if (stream) std::cout << '\n';
            std::cout << "请求编号：" << context.request_id << '\n'
                << "类别：" << gateway::taskName(result.analysis.task) << '\n'
                << "首选模型：" << result.decision.model_id << '\n'
                << "实际调用：" << actual.model_id << '\n'
                << "理由：" << result.decision.reason << '\n'
                << "备用切换：" << (result.fallback_used ? "是" : "否") << '\n';
            if (result.fallback_used) std::cout << "切换原因：" << result.fallback_reason << '\n';
            for (const auto& attempt : result.attempts)
                std::cout << "尝试：" << attempt.model_id << "，HTTP " << attempt.http_status
                    << "，" << gateway::errorName(attempt.error_code) << "，" << attempt.elapsed_ms << "ms\n";
            if (!result.ok()) throw std::runtime_error(actual.error_message);
            if (!stream) std::cout << "回答：" << actual.answer << '\n';
            std::cout << '\n';
        };
        if (single_message) show(message);
        else if (interactive) {
            std::cout << "输入问题，空行退出（使用UTF-8终端）：\n";
            std::string question;
            while (std::getline(std::cin, question) && !question.empty()) {
                show(question);
            }
        } else {
            show("帮我检查这段C++代码");
            show("翻译这段Python注释");
            show("介绍一下计算机缓存");
        }
    } catch (const std::exception& error) {
        std::cerr << "错误：" << error.what() << '\n';
        return 1;
    }
    return 0;
}
