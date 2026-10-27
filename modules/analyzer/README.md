# B：问题分类（V0.1）

本模块将原 `classifier.cpp` 的规则表提取为可独立链接的 C++17 库。
业务代码只依赖标准库，不依赖 Qt、JSON、网络或其他同学的模块。

## 公共接口 1.0

公共类型与声明统一位于 `shared/contracts.h` 和 `shared/module_api.h`。
本目录的 `question_analyzer.h` 保留 `classification` 命名空间别名，兼容原有演示和测试。

```cpp
#include "question_analyzer.h"

const auto analysis = gateway_contract::analyzeQuestion("翻译这段 C++ 报错");
// analysis.task == "translation"
// analysis.reason 解释明确翻译意图和命中的规则
// analysis.uncertain == false
```

| 字段 | 类型 | 约定 |
| --- | --- | --- |
| `task` | `std::string` | 只返回 `programming`、`math`、`translation`、`writing`、`general` |
| `reason` | `std::string` | UTF-8 可读文字，说明命中的关键词、结构规则或回退原因 |
| `uncertain` | `bool` | 为 `true` 时，`task` 固定为 `general` |

输入为 UTF-8 问题字符串。B 不修改输入，不读写文件、不发送请求，不选择或调用模型。
空输入或仅含 ASCII 空白时返回 `general`、非空回退理由和 `uncertain=true`。
HTTP 入口仍由 A 负责拒绝空问题；该返回值是 B 的独立防御行为。
调用方负责传入合法 UTF-8；本模块不新增编码错误类型。正常分类结果通过返回值提供，标准库资源异常由调用方统一处理。

规则表首次调用时初始化，后续只读，支持并发调用。程序初始化会构造正则表达式；未对分类耗时作性能承诺。

## 判断顺序

1. 空输入回退为不确定的通用问答。
2. 明确的翻译动作优先于被翻译内容中的编程、数学关键词；明确的写作、润色动作同理。
3. 同时出现明确翻译和写作动作时，V0.1 保守返回 `general/uncertain=true`。
4. 其余按原规则表的关键词和正则权重打分。中性动作“帮我写/写”不单独加分，靠函数、作文等宾语区分。
5. 多个专门任务最高分相同，或通用得分超过专门证据时，返回不确定的通用问答。
6. 未命中专门规则、但命中“为什么”等通用问答线索时，返回 `general/uncertain=false`。
7. 完全无明确线索时，返回 `general/uncertain=true`，说明未匹配规则。

与通用基线同分时，唯一命中的专门任务可以胜出；多个专门任务同分则保守回退。
例如“这个单词怎么读”仍归翻译；“C++ 方程”归不确定的通用问答。
原规则分数是人工预设的证据权重，不是分类正确率；当前公共接口不把旧版的得分占比当作概率输出。

## 独立构建与运行

在仓库根目录执行：

```powershell
cmake -S modules/analyzer -B modules/analyzer/build -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build modules/analyzer/build
ctest --test-dir modules/analyzer/build --output-on-failure
.\modules\analyzer\build\analyzer_demo.exe
.\modules\analyzer\build\analyzer_demo.exe "你好"
.\modules\analyzer\build\classifier.exe --test
```

`analyzer_demo` 默认演示五类各一条；`classifier --test` 运行原有 21 条回归样例；
`analyzer_tests` 检查标准输出、理由、不确定回退、确定性和 8 个并发任务的 128 次调用。
Windows 命令行参数直接读取 Unicode 并转成 UTF-8，避免按本机代码页二次猜测编码。
标准输入由调用方提供 UTF-8。

若由其他 CMake 工程接入，可添加本目录并链接 `question_analyzer` 目标；
若手动编译，加入 `question_analyzer.cpp` 和本目录头文件搜索路径。
`cli_support.h` 及 `shell32` 仅用于独立演示和测试，网关调用业务接口不需要它们。

## 与其他模块的边界

- A 负责读取问题、拒绝空输入，并在后续版本将结果写入 HTTP 响应。
- C 可以直接将 `analysis.task` 作为现有 `routing::selectModel` 的任务字符串输入。
- D 接收选定模型配置和问题，无需依赖 B 的类或头文件。
- D 提交包中现有的 `gateway::TaskType/AnalysisResult` 属于内部框架类型；接入时按公共约定作命名映射，不能直接转换枚举序号。

| B 标准任务名 | 现有框架中的枚举名称 |
| --- | --- |
| `programming` | `gateway::TaskType::Coding` |
| `math` | `gateway::TaskType::Math` |
| `translation` | `gateway::TaskType::Translation` |
| `writing` | `gateway::TaskType::Writing` |
| `general` | `gateway::TaskType::General` |

公共接口 1.0 已按 A 的本次授权确定为接入基线，完整规则见 `docs/公共接口_v1.0.md`。
B 已采用唯一公共结构；C/D 可保留内部类型，在边界增加转换，不需要重写业务实现。

## 已知范围

规则会受单字关键词、否定表达、引述内容和复杂混合意图影响；回退规则不代表所有歧义都能被检测。
V0.2 的 30 条冻结题集、更多混合题修正和 HTTP 网关接入继续按开发计划完成。
本次不把原有 21 条回归样例与 24 条接口样例合称为已经完成 V0.2 的冻结测试集。
