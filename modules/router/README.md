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

## 后续工作

JSON 读取、手动模式、首选/备用候选、配置保存及真实模型调用字段尚未实现。当前未接入网关；公共接口、字段及模块位置仍需 A/C/D 确认。CMake 入口已提供，本次实际验证使用上述 g++ 命令。
