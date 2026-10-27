# 多模型智能路由网关：B V0.3 与 C 分支整合

2026-10-27：以 A 网关交接版为基础，合入 C 的 `codex/c-routing-v1`（`164a3e6`），补齐 B 的 V0.3 分类、冻结题集、规则说明和结果记录。

- [本轮验收结果与剩余边界](docs/B模块V0.3与C接入验收.md)。
- [A+B+C 联调构建与运行说明](integration/bc/README.md)。
- [B 分类规则与使用方法](modules/analyzer/README.md)。
- [30 条逐题实际结果](docs/B_v0.3_实际结果.tsv)。

推荐从本目录运行统一检查：

```powershell
cmake -S integration/bc -B build-bc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build-bc --parallel 3
ctest --test-dir build-bc --output-on-failure
```

编译器路径是本机示例，其他电脑替换成自己的 C++17 编译器。输出目录 `build-bc/bin/` 含分类演示、评测程序和两个明确命名的测试服务器。
根目录的 `CMakeLists.txt` 仍是 E 的 Qt 工程；上面的命令使用单独的联调入口。

11 组检查全部通过。联调运行实际 A/B/C，仅 D 使用测试替身，不调用真实模型，也没有替换 E 的窗口代码。

当前模块接入基线为 **公共接口 1.0**，由 A 维护：

- [先看白话说明：五个人怎么合作、接口是什么意思、每个人先做什么](docs/公共接口_v1.0.md)。
- [写代码时查：具体字段、函数和消息格式](docs/公共接口_v1.0_代码细则.md)。
- [公共数据结构](shared/contracts.h) 与 [B/C/D 函数声明](shared/module_api.h)。
- [模型配置示例](config/models.example.json)。
- [接口验证入口](shared/README.md)：可独立构建，包含 B 实际调用与原有样例。

B/C 已接入公共类型并完成 HTTP 联调；C 的独立验收方法见 [C 模块记录](docs/C模块公共接口与验收.md)。E 的客户端更新已从 `main` 取回，Qt 全流程仍待实际联调；D 仍需提供公共 `callModel`，真实模型调用尚未验收。

## B 模块

纯 C++17 规则分类器，将 UTF-8 问题转换为标准任务类型、判断理由和不确定标志。
原有规则已提取为可复用模块，并已接入 A 网关与 C 的实际路由。

```text
问题字符串 → classification::analyzeQuestion → task / reason / uncertain
```

## 分类模块文件

- [公共头文件](modules/analyzer/question_analyzer.h)：纯标准库接口，输出五类标准英文任务名。
- [分类实现](modules/analyzer/question_analyzer.cpp)：关键词、正则加权打分、明确意图优先和保守回退。
- [独立演示](modules/analyzer/demo.cpp)：默认打印五类问题的实际结果，也可传入一个问题。
- [接口与回退测试](modules/analyzer/tests.cpp)：46 条样例，以及 8 个并发任务的 128 次调用检查。
- [冻结题集](modules/analyzer/fixtures/v03_frozen_30.tsv)：五类各五条明确问题，加五条混合或模糊问题。
- [评测程序](modules/analyzer/evaluation.cpp)：记录预期、实际类型、不确定标志、理由及通过状态，并检查重复调用一致性。
- [原命令行入口](classifier.cpp)：转到模块内的 `classifier_cli.cpp`，保留逐行输入和原有 21 条回归样例。
- [构建、规则和对接说明](modules/analyzer/README.md)。
- [B 接口与 V0.1 验收记录](docs/B模块接口与V0.1验收.md)。

## 构建与运行

在本仓库目录执行（Windows、MinGW g++、CMake、Ninja）：

```powershell
cmake -S modules/analyzer -B modules/analyzer/build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build modules/analyzer/build
ctest --test-dir modules/analyzer/build --output-on-failure
.\modules\analyzer\build\analyzer_demo.exe
.\modules\analyzer\build\classifier.exe --test
.\modules\analyzer\build\classifier.exe "翻译这段 C++ 报错"
```

也可直接编译命令行入口（先建立输出目录）：

```powershell
New-Item -ItemType Directory -Force modules/analyzer/build | Out-Null
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 classifier.cpp modules/analyzer/question_analyzer.cpp -lshell32 -o modules/analyzer/build/classifier.exe
```

Linux 等平台去掉 `-lshell32`，执行文件没有 `.exe` 后缀。
仓库原有的根目录 `classifier.exe` 是旧版本产物，本次未覆盖；请运行构建目录内的新程序。
原 `.docx` 原理说明保留为旧版资料，当前接口和回退规则以模块说明及源码为准。

本次完成 B 在 V0.2/V0.3 中要求的分类交付与 A+B+C 接入。原 V0.1 说明属于历史验收资料；完整项目的真实调用及 Qt 验收仍需 D/E 配合。
