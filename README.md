# 多模型智能路由网关 · 模拟演示版

现在可以完成整条流程：在窗口提问 → 判断题型 → 按能力分选择模型 → 通过本机 HTTP 服务取得模拟回答 → 显示结果。

**目前没有真实模型账号。这个版本的回答是模拟文字，不代表 AI 已经真实解题。** 真实模型调用和另一台电脑上的验证仍待完成，原计划里包含真实调用的里程碑不能标为全部通过。

## 先运行

使用整理好的 Windows 运行包，完整解压后双击 `MyClient.exe`。程序会自动启动随附网关，无需开终端、安装 Qt 或填写密钥。不要只复制一个 exe。

窗口可操作：

- 自动模式：输入“解释 C++ 指针”“求解方程 x+2=5”“把你好翻译成英文”“写一封请假信”，观察不同题型及选模。
- 手动模式：在顶部指定模型，当前问题由该模型回答。
- 故障演示：勾选“让首选模型失败”。自动模式最多换一个备用；手动模式直接提示失败。
- 模型设置：修改启用状态或能力分并保存，下一个问题生效，重新打开仍保留。

详细演示和验证范围见 [模拟演示验收](docs/模拟演示验收.md)。

## 代码对应谁的工作

| 部分 | 当前接入位置 | 做什么 |
| --- | --- | --- |
| A 网关 | `gateway/` | 接收窗口消息，安排分类、选模、调用，处理备用切换和日志 |
| B 分类 | `modules/analyzer/` | 给出编程、数学、翻译、写作或通用类型，以及理由 |
| C 选模 | `modules/router/` | 读取、保存模型设置；复用原选模规则挑首选和备用 |
| D 调用 | `modules/providers/public_provider.cpp`、`ai-gateway/src/providers/` | 将公共格式转换后，复用已有 libcurl 调用代码 |
| E 窗口 | `mainwindow.cpp` | 提问、选择模型、显示回答及错误、修改设置 |

公共交接约定仍为 [公共接口 1.0](docs/公共接口_v1.0.md)，数据结构在 `shared/contracts.h`，函数声明在 `shared/module_api.h`。B/C/D 的公共函数已接入整合程序。

每次问题独立处理，不发送聊天历史。能力分是项目预设值，不是对真实模型能力的测评。

## 修改代码后构建

需要 C++17 编译器、CMake、Python 3、Qt 6（Widgets/Network/Test）和 libcurl 开发包。Windows 使用与 Qt 相匹配的 MinGW。本机已用 Qt 6.11.2 与 MinGW 13.1 验证。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.11.2/mingw_64 -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build --parallel 3
ctest --test-dir build --output-on-failure
.\build\bin\MyClient.exe
```

如果 libcurl 不在系统搜索目录，配置时补充 `-DCURL_INCLUDE_DIR=头文件目录` 和 `-DCURL_LIBRARY=导入库文件`。Windows 构建会自动复制找到的 libcurl 依赖并执行 Qt 运行库整理；不要混用其他编译器版本的运行库。

只构建网关可加 `-DBUILD_CLIENT=OFF`。网关单独启动：

```powershell
.\build\bin\gateway.exe --demo --port 8080 --config .\config\models.example.json --log .\build\requests.jsonl
```

检查入口为 `GET /health`、`GET /api/models`、`POST /api/chat`。本版增加的设置及故障演示消息见 [整合说明](docs/模拟演示验收.md)。

## 整理运行包

```powershell
python tools/package_demo.py --bin build/bin --out dist/AI网关模拟演示
```

脚本只打包运行文件、默认模型设置和白话使用说明，不包含测试程序、请求日志或密钥。源码与运行包应分别保存。

历史阶段计划保留在 [两周开发与验收计划](docs/两周开发与验收计划.md)。当前进度以 [模拟演示验收](docs/模拟演示验收.md) 为准。
