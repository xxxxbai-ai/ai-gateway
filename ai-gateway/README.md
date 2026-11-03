# AI Gateway：框架与设计概要

目标：根据问题选择回答模型。当前是能编译运行的 C++17 工程：D 的 provider 层已支持模拟/真实模型、流式解析、超时/取消和备用事实报告；依赖 libcurl、nlohmann/json，模拟与测试使用 cpp-httplib。HTTP/Qt 的整链集成仍由 A/E 推进。

```text
客户端 → GatewayService::handle
          ├─ QuestionAnalyzer::analyze：识别问题类型
          ├─ ModelRepository::listModels：读取模型配置
          ├─ Router::select：选择模型
          └─ ProviderAdapter::invoke：调用并返回回答
```

## 五人分工

| 成员 | 目录 | 主要编码任务 |
| --- | --- | --- |
| A | `src/server/`、`src/common/` | 网关调度、HTTP入口、公共类型 |
| B | `src/analyzer/` | 问题分类、上下文分析 |
| C | `src/router/`、`src/storage/` | 模型能力配置、选模、SQLite存储 |
| D | `src/providers/` | 模型接入、流式解析、模拟服务 |
| E | `src/desktop/` | Qt页面、异步调用与回答展示 |

各目录已有关键接口或示例实现，后续任务用对应成员的TODO标注。E的GatewayClient目前只有接口，没有Qt实现。

## 关键接口

| 接口 | 输入 | 输出 |
| --- | --- | --- |
| `analyze(request)` | 用户问题 | 问题类别与分析理由 |
| `select(request, analysis, models)` | 请求、分析结果、模型配置 | 模型ID与选择理由 |
| `invoke(model_id, request)` | 指定模型、请求 | 回答文本 |
| `handle(request)` | 问题、`auto`或具体模型ID | 类别、实际模型、回答、理由 |

公共类型见`src/common/contracts.h`。当前请求只有单轮question，后续再扩展messages；适配器不负责选模或自动重试。

后续HTTP入口只需先实现以下三个接口（当前尚未监听端口）：

- `GET /health`：运行状态。
- `GET /v1/models`：可用模型列表。
- `POST /v1/chat/completions`：解析请求，调用handle，转换响应；对外messages转换为内部请求。

## 构建与运行

本机已安装的MSYS2工具链可直接使用：

```powershell
cmake -S . -B build -G Ninja
cmake --build build
.\build\gateway_demo.exe
.\build\gateway_demo.exe --interactive
```

从项目目录执行。其他环境使用CMake和支持C++17的编译器即可；Visual Studio多配置构建的可执行文件通常位于`build/Debug/`。

默认演示依次选择`demo-code`、`demo-language`、`demo-general`；交互模式可输入问题。模拟模型只回显收到的问题，不生成真实AI答案。模型能力分是人工预设的演示数据，关键词分类也只是规则逻辑，不代表真实质量。

开发顺序：D 的 provider 层、公共 `callModel`、流式/非流式、超时取消、模拟故障和真实模型适配已完成，并有 9 组测试；A 接 HTTP 网关、E 接 Qt，SQLite 持久化和多轮上下文仍待推进。详细验收要求沿用桌面需求说明书。
