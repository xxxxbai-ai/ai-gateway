# C：模型管理与路由（V0.1）

独立 C++17 模块，无 Qt 或网络依赖。接口为 C 模块内部草案，接入前需与 A、D 确认。

## 文件

- `model_router.h`：任务名称、模型配置、选择结果及函数声明。
- `model_router.cpp`：三个模拟模型能力表和自动选模。
- `demo.cpp`：独立演示入口。
- `tests.cpp`：基本选模与边界检查。

## 运行（仓库根目录，PowerShell）

确保 C++17 编译器在 PATH 中；本机已用 D:/msys64/ucrt64/bin/g++.exe 验证。

```powershell
New-Item -ItemType Directory -Force modules/router/build | Out-Null
g++ -std=c++17 -Wall -Wextra -pedantic modules/router/model_router.cpp modules/router/demo.cpp -o modules/router/build/router_demo.exe
./modules/router/build/router_demo.exe
./modules/router/build/router_demo.exe programming math

g++ -std=c++17 -Wall -Wextra -pedantic modules/router/model_router.cpp modules/router/tests.cpp -o modules/router/build/router_tests.exe
./modules/router/build/router_tests.exe
```

也可单独用 CMake 构建（需已配置可用的编译器和构建工具）：

```powershell
cmake -S modules/router -B modules/router/build-cmake
cmake --build modules/router/build-cmake
ctest --test-dir modules/router/build-cmake -C Debug --output-on-failure
```

## 规则

评分是项目人工预设值，不能代表真实模型质量。

| 模型 | 编程 | 数学 | 翻译 | 写作 | 通用 |
| --- | ---: | ---: | ---: | ---: | ---: |
| mock_code | 95 | 70 | 55 | 60 | 70 |
| mock_math | 60 | 95 | 55 | 60 | 70 |
| mock | 65 | 65 | 90 | 90 | 90 |

只从启用、当前任务评分存在且在 0–100 范围内的模型中选择最高分，同分按编号字典序升序选择，不依赖列表输入顺序。未知任务类型返回错误，不擅自改为 general；模糊问题应由 B 分类后传入 general。空模型列表或全部不合格时失败。空编号或重复编号导致配置错误。

成功返回模型编号及理由；失败返回 error，selected_model 为空。模块只选模型，不执行调用。

## 实际验证记录

2026-10-20，Windows，g++，C++17；开启 Wall/Wextra/pedantic 后编译成功，无警告。

演示实际输出与预期一致：

```text
programming -> mock_code
math -> mock_math
translation -> mock
writing -> mock
general -> mock
```

每项同时输出选模理由和评分。边界检查实际输出 `All routing checks passed.`，覆盖五类结果、未知类型、空列表、同分、列表顺序改变、全部禁用、评分缺失/越界、重复/空编号。

## 中文乱码

Windows 演示程序启动时会将控制台输出编码设为 UTF-8。若使用 PowerShell 的输出捕获或管道仍显示乱码，先执行以下命令再运行程序：

```powershell
chcp 65001
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
./modules/router/build/router_demo.exe
```

编译器也应使用 UTF-8：MSVC 的 CMake 配置已指定 `/utf-8`；g++ 可显式添加 `-finput-charset=UTF-8 -fexec-charset=UTF-8`。终端字体需支持中文。

## 公共接口 1.0 适配

`contract_adapter.cpp` 实现 `shared/module_api.h` 中 C 负责的三个入口：`loadModels`、`saveModels`、`selectModel`。旧的 `routing::selectModel` 与演示程序保留。公共配置使用 `config/models.example.json` 的格式，含五类 0–100 整数评分、公共编号和上游连接字段。读取失败会返回 `invalid_config`，不会回退到内置配置；保存先写同目录临时文件，再替换目标文件。

自动模式按任务评分降序、同分按编号升序，返回首选及至多一个不同备用。手动模式只返回指定的启用模型。是否在失败后调用备用由 A 决定，C 不执行模型调用。

在仓库根目录运行：

```powershell
cmake -S modules/router -B modules/router/build-cmake -G Ninja
cmake --build modules/router/build-cmake
ctest --test-dir modules/router/build-cmake --output-on-failure
```

本机若 CMake 找不到 MSYS2 依赖，可在配置命令追加 `-DCMAKE_PREFIX_PATH=D:/msys64/ucrt64`。新测试读取示例配置，检查自动及手动选择、错误码、配置保存与重载、配置变化后的路由。

仍需 A 将公共 C 入口接进网关；真实模型的可用性与调用结果由 D 验证。

### 给 A 的接入与演示

包含 `shared/module_api.h`，链接 CMake 目标 `model_router`。一次聊天请求先调用 `loadModels(path)` 取得配置快照，再把同一份 `models` 传给 `selectModel(analysis, request, models)`；按其 `primary_model` 查找完整 `ModelConfig` 交给 D。仅满足公共接口规定的切换条件时使用 `fallback_model`。模型列表请求也重新加载配置；保存使用 `saveModels(path, models)`，下一次请求再加载。三个函数失败时均检查 `success/error.code/error.message`，不要用隐式默认模型代替错误。该接口不执行上游调用，也不判断真实模型是否可达。

可复现演示（仓库根目录）：

```powershell
cmake -S modules/router -B modules/router/build-cmake -G Ninja
cmake --build modules/router/build-cmake
ctest --test-dir modules/router/build-cmake --output-on-failure
.\modules\router\build-cmake\router_contract_demo.exe config/models.example.json
```

演示会在系统临时目录复制配置，依次打印数学任务自动首选 `mock_math`、备用 `mock_code`，手动选择 `mock_code`，禁用的 `real_a` 返回 `disabled_model`；将 `mock_math` 禁用并保存、重新读取后，自动首选变为 `mock_code`、备用变为 `mock`。原始示例配置不会被修改。`router_contract_checks` 还检查未知模型、错误模式、重复编号、全部禁用和保存失败不损坏原文件。

当前 `ai-gateway/` 内仍使用旧的 `gateway::ModelRepository` 和 `gateway::Router`，其评分为 0–5 数组；A 接入时应直接使用公共 0–100 `scores`，不要按数组位置或缩放分数转换。HTTP `/api/models` 只输出启用模型的 `id/name/is_mock`，不输出 `base_url/credential_ref`。公共错误码与 HTTP 状态对应关系见 `shared/contracts.h`。
