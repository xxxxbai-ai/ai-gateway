# C 模块：公共接口 1.0 与独立验收

验证日期：2026-10-27。代码位于 `modules/router/`；正式接口声明见 `shared/module_api.h`，配置样例见 `config/models.example.json`。

## 已完成

`gateway_contract::loadModels` 从 JSON 读取并校验配置，失败返回 `invalid_config`；`saveModels` 先写临时文件再替换，失败不破坏原文件；`selectModel` 支持自动首选及至多一个不同备用、手动指定、禁用和未知模型错误。自动模式按任务评分降序选择，同分按公共模型编号排序。所有评分均为人工预设，不表示真实模型质量。

## 复现命令与实际结果

从仓库根目录运行：

```powershell
cmake -S modules/router -B modules/router/build-cmake -G Ninja
cmake --build modules/router/build-cmake
ctest --test-dir modules/router/build-cmake --output-on-failure
.\modules\router\build-cmake\router_contract_demo.exe config/models.example.json
```

本次使用 Windows、g++ 16.1.0、CMake 4.1.2、Ninja 1.13.2 构建成功；CTest 2/2 项通过。演示输出的关键值：

| 场景 | 实际结果 |
| --- | --- |
| 数学任务自动选择 | 首选 `mock_math`，备用 `mock_code` |
| 手动指定 `mock_code` | 首选 `mock_code`，无备用 |
| 手动指定禁用的 `real_a` | `disabled_model` |
| 禁用 `mock_math`，保存并重新读取 | 首选 `mock_code`，备用 `mock` |

演示仅修改系统临时目录中的配置副本。测试另覆盖未知模型、无效模式、重复编号、全部禁用、保存失败后的原文件完整性。

## 交给 A 的接入点

包含 `shared/module_api.h` 并链接 `model_router`。每次聊天请求调用 `loadModels` 获取一份配置快照，用这份快照调用 `selectModel`，再按返回编号找完整 `ModelConfig` 传给 D。模型列表请求也重新读取配置。配置保存后，下一次请求重新读取便可生效。三个接口的失败均返回结构化错误，A 按 `shared/contracts.h` 映射 HTTP 状态。具体调用顺序见 `modules/router/README.md`。

当前网关仍使用旧的 `gateway::Router/ModelRepository`，尚未调用 C 的公共接口；HTTP 端到端结果、真实模型可用性及 Qt 展示不能凭此独立验收记录判定通过。A 接入后需核对 HTTP 返回的任务、首选或实际模型、理由和配置变化后的新选模结果。
