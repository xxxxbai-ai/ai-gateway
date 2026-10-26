# 公共接口 1.0

- `contracts.h`：唯一公共数据结构、任务名称、错误码与备用策略辅助函数。
- `module_api.h`：B/C/D 的标准函数声明。
- `examples/contract_example.cpp`：使用公共类型和 B 实际分类函数的示例。
- `tests/contracts_test.cpp`：声明可编译性、B 兼容入口和策略边界检查。
- [完整约定及各成员迁移清单](../docs/公共接口_v1.0.md)。

在仓库根目录运行：

```powershell
cmake -S shared -B shared/build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build shared/build
ctest --test-dir shared/build --output-on-failure
.\shared\build\contract_example.exe
```

B 已使用本接口；C/D 仍需实现或包装标准签名。示例不伪造 C/D 调用结果。
其他工程可以添加 `shared/` 为头文件搜索路径，再链接相应模块的实际实现。
本目录 CMake 是独立的接口验证入口，会一起编译 B 的实现和样例。
