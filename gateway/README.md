# A 网关交接版

这次只提交 A 负责的网关、A 维护的公共交接约定及 A 的测试。
代码从团队主分支单独整理，B 的分类、C 的选模、D 的调用、E 的窗口实现保持主分支原样。
各负责人按 [分工交接清单](../docs/A网关交接说明.md) 修改自己的代码，再交给 A 联调。

本目录的 `Service::chat` 已写好：输入检查 → B 分类 → C 选模 → D 调用 → 错误与备用处理 → 返回窗口。
它调用 `shared/module_api.h` 声明的公共函数；这些函数的实际实现由 B/C/D 提供。

## A 的文件

| 文件 | 用途 |
| --- | --- |
| `main.cpp` | 启动 HTTP 服务，接收和回复请求 |
| `service.h`、`service.cpp` | 问答、模型列表、设置和日志处理 |
| `cli_arguments.h` | 网关自己的中文启动参数处理 |
| `CMakeLists.txt` | A 的独立构建入口，不替换团队根目录构建文件 |
| `tests/` | 只检查 A 的处理行为，使用可控制的 B/C/D 测试替身 |
| `../shared/contracts.h`、`../shared/module_api.h` | A 维护的交接类型和函数声明，不含分类、排序、模型调用实现 |

## 先验证 A 自己的部分

需要 C++17 编译器、CMake、Python 3 和团队主分支已有的头文件依赖。无需 Qt、libcurl 或真实模型账号。
Windows 请使用自己的编译器路径；以下为本机已验证的例子。

```powershell
cmake -S gateway -B build-a -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build-a --parallel 3
ctest --test-dir build-a --output-on-failure
```

默认生成 `gateway_a` 库、`gateway_a_tests` 与 `gateway_test_host`。
`gateway_test_host` 是 A 的测试服务器，使用固定返回值；它不运行同学的分类、选模或网络调用，不能作为全组联调成功的证据。
默认不生成正式 `gateway.exe`，以免把测试替身误当成大家已接入的功能。

本次两组检查通过：处理逻辑检查与 HTTP 收发检查。测试覆盖输入校验、参数转交、实际结果回传、429/503 备用、手动不切换、至多两次、共享时限、设置转交及错误转换。
测试替身里的保存只更新内存；磁盘保存、分类准确性和真实网络调用仍需各负责人自行验证。

## B/C/D 实现齐全后构建正式网关

在配置时通过 `GATEWAY_MODULE_SOURCES` 传入组员提供的实现文件列表，或通过 `GATEWAY_MODULE_LIBRARIES` 传入其构建目标/库。
需要先定义组员的构建目标时，可用 `GATEWAY_MODULES_CMAKE` 指定团队另行维护的 CMake 文件。
它们必须真正实现公共头文件列出的五个函数，并提供各自内部代码与依赖；不能只传入声明。

这几项都为空时只构建 A 和测试；填写后才生成正式 `gateway.exe`。组员的包装与构建文件由本人维护，本次不代写。

正式启动示例：

```powershell
.\build-a\bin\gateway.exe --port 8080 --config C提供的模型配置.json --log .\build-a\requests.jsonl
```

D 的模拟服务或真实服务由 D 提供和启动。本版网关不拥有模拟服务器，不改变模型地址，不提供 `--demo` 或界面故障开关。
失败场景由 D 的测试服务提供，再由 A 验证备用规则。真实模型账号目前暂缺，先完成各自公共入口与模拟对接。

## 给 E 的 HTTP 入口

- `GET /health`：检查网关运行。
- `GET /api/models`：取得当前启用模型的 `id/name/is_mock`，由 C 配置产生。
- `POST /api/chat`：接收 `mode/model/message`。成功回复包含 `selected_model`，没有名为 `model` 的返回字段。
- `GET /api/config`、`POST /api/config`：取得或更新模型启停与五类能力分。A 校验后交给 C 保存，不暴露服务地址和凭证引用。

每次问题独立处理，不发送聊天历史。自动最多尝试两个不同模型；手动失败直接返回。
详细消息样例和每个人需要处理的问题见 [分工交接清单](../docs/A网关交接说明.md)。
