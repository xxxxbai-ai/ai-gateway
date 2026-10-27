# 多模型智能路由网关：公共接口与 B 模块

当前模块接入基线为 **公共接口 1.0**，由 A 维护：

- [先看白话说明：五个人怎么合作、接口是什么意思、每个人先做什么](docs/公共接口_v1.0.md)。
- [写代码时查：具体字段、函数和消息格式](docs/公共接口_v1.0_代码细则.md)。
- [公共数据结构](shared/contracts.h) 与 [B/C/D 函数声明](shared/module_api.h)。
- [模型配置示例](config/models.example.json)。
- [接口验证入口](shared/README.md)：可独立构建，包含 B 实际调用与原有样例。

B 已接入公共类型；C 的公共路由与配置接口已实现并通过独立测试，验收方法见 [C 模块记录](docs/C模块公共接口与验收.md)。A 尚需把 C 接入网关，D 的公共适配及项目完整联调仍待验收。

## B 模块

纯 C++17 规则分类器，将 UTF-8 问题转换为标准任务类型、判断理由和不确定标志。
原有规则已提取为可复用模块，供后续 HTTP 网关接入。

```text
问题字符串 → classification::analyzeQuestion → task / reason / uncertain
```

## V0.1 交付

- [公共头文件](modules/analyzer/question_analyzer.h)：纯标准库接口，输出五类标准英文任务名。
- [分类实现](modules/analyzer/question_analyzer.cpp)：关键词、正则加权打分、明确意图优先和保守回退。
- [独立演示](modules/analyzer/demo.cpp)：默认打印五类问题的实际结果，也可传入一个问题。
- [接口与回退测试](modules/analyzer/tests.cpp)：24 条样例，以及并发调用检查。
- [原命令行入口](classifier.cpp)：保留逐行输入和原有 21 条回归样例，使用同一个公共分类函数。
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

本次交付已完成 B 的独立模块和公共接口适配；其他模块按公共接口 1.0 接入。
实际接入 HTTP 网关与 Qt 的整合验收属于 V0.2。
