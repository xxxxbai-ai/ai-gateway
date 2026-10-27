# A+B+C 联调入口

本入口链接实际的 A 网关、B 分类和 C 新分支实现。D 尚未提供公共 `callModel`，这里用一个仅存在于测试目标中的 D 替身，不修改 D/E 的实现文件。

## 构建和检查

在团队工程根目录，也就是本轮的 `v03-integration/` 内执行：

```powershell
cmake -S integration/bc -B build-bc -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=C:/Qt/Tools/mingw1310_64/bin/g++.exe
cmake --build build-bc --parallel 3
ctest --test-dir build-bc --output-on-failure
```

编译器路径是本机示例；其他电脑换成自己的 C++17 工具链。运行时使用该编译器配套的运行库。A 的构建会将本机 MinGW 的三项运行库复制到 `bin/`。
Python 测试会启动随机端口的本机服务器，完成后自动关闭；不占用固定的 8080 端口。

## 各程序是什么

| 程序 | 作用 |
| --- | --- |
| `bin/analyzer_demo.exe` | B 的五类演示，或接收一个问题并打印分类与理由 |
| `bin/classifier.exe` | B 的历史命令行界面，`--test` 运行 21 条旧样例 |
| `bin/analyzer_evaluation.exe` | 运行固定 30 条题并输出逐题 TSV |
| `bin/router_tests.exe`、`router_contract_tests.exe` | C 自己的两组测试 |
| `bin/gateway_test_host.exe` | A 自己的测试服务器；B/C/D 全是 A 的测试替身 |
| `bin/gateway_bc_test_host.exe` | 本轮新增的联调测试服务器；实际 A/B/C，仅 D 是测试替身 |

两个测试服务器名称不同、启动时也有不同提示。它们都不能作为真实模型成功记录。
本入口不生成正式的 `gateway.exe`；正式服务器要在 D 的公共调用函数交付后按 `gateway/README.md` 链接实际 B/C/D。

## 联调测试验证什么

`http_tests.py` 先独立运行 B 的冻结题评测，再让相同的 30 个问题走 A 的 HTTP 接口，逐条对照分类类型、不确定标志、B 理由、C 选择的模型和 A 的日志。

其余检查包括：

- 中文、换行、引述、否定和英文大小写经过 HTTP 后仍得到预期分类。
- C 选出的首选明确返回模拟 429/503 时，A 只调用一个不同备用；手动失败不换模型。
- 首选及备用均失败时不调用第三个；未知、禁用、无可用模型和空输入不进入 D。
- 经 A 设置接口调用 C 保存评分和启停状态，新问题立即按新配置路由，重启服务器后仍保留。
- 无效保存保留旧配置，损坏配置返回明确错误；中文配置和日志文件名可以使用。
- 超时和网络错误模拟不触发额外重试；失败后下一次请求仍可成功。
- 并发请求的编号、分类、选模与日志互不串用。

测试配置由脚本在临时目录创建，所有模拟项明确改为 `bc-test-*`。D 替身拒绝真实模型和未标记配置，不会发送网络请求。

## 下一步怎样换成正式 D

保留 B 的 `question_analyzer` 和 C 的 `model_router` 构建目标，按 A 的构建扩展入口提供它们，再链接 D 实现的公共 `callModel`。
不要把 `provider_test_double.cpp` 加到正式源文件清单，也不要把 B 的 `classifier_cli.cpp` 加进服务器。
C 使用的是 `codex/c-routing-v1` 的原始实现；本轮没有改动 C 的算法或配置代码。
