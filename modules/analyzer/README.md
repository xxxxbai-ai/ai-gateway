# B：问题分类（V0.3）

2026-10-27：本轮已补齐公共入口、30 条冻结题、常见中文及大小写处理、混合/模糊回退、规则说明和实际结果，并通过 A+B+C 的 HTTP 联调。
完整结果见 [验收记录](../../docs/B模块V0.3与C接入验收.md)，逐题数据见 [实际结果](../../docs/B_v0.3_实际结果.tsv)。

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
空输入或仅含 ASCII 空白、全角空格、NBSP 时返回 `general`、非空回退理由和 `uncertain=true`。
HTTP 入口仍由 A 负责拒绝空问题；该返回值是 B 的独立防御行为。
调用方负责传入合法 UTF-8；本模块不新增编码错误类型。正常分类结果通过返回值提供，标准库资源异常由调用方统一处理。

规则表首次调用时初始化，后续只读，支持并发调用。程序初始化会构造正则表达式；未对分类耗时作性能承诺。

## 判断顺序

1. 将 ASCII 字母转为小写，识别 ASCII 空白、全角空格与 NBSP；空白输入回退为不确定的通用问答。
2. 屏蔽“不要翻译／不用润色／do not translate／don't translate”等明确局部否定。动作检测跳过成对中文、英文引号及反引号中的内容，避免把引述动作当成外层要求。
3. 同时发现有效的翻译和写作动作时，保守返回 `general/uncertain=true`，说明主任务不明确。
4. 单一明确翻译动作优先，支持“译为”；单一明确写作动作其次，支持“写一封请假信”和英文 `write an email` 等文体请求。
5. 明确“写个函数／编写程序／implement／debug”时按编程判断；数学关键词可作为要编写的程序的内容。
6. 其余按原关键词和正则权重打分。“帮我写/写”不单独加分；去除容易误伤“人类／请求”等普通表达的单字规则。英文关键词检查词边界，避免把 `trust/catalog` 当成 `rust/log`。
7. 多个专门任务最高分相同，或通用得分超过专门证据时，返回不确定的通用问答。
8. 未命中专门规则但有“为什么”等明确通用线索时返回 `general/uncertain=false`；完全无明确线索时返回 `general/uncertain=true` 并说明回退原因。

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
.\modules\analyzer\build\analyzer_evaluation.exe modules/analyzer/fixtures/v03_frozen_30.tsv modules/analyzer/build/v03_actual_results.tsv
```

`analyzer_demo` 默认演示五类各一条；`classifier --test` 运行原有 21 条回归样例；
`analyzer_tests` 检查 46 条接口与边界样例、理由、不确定回退、确定性和 8 个并发任务的 128 次调用。
`analyzer_evaluation` 独立读取固定的 30 条题集，检查预期任务、不确定标志、理由依据及四次调用的一致性，输出可复查 TSV。
冻结题集是五类各 5 条明确问题，加 5 条混合或模糊问题；没有用旧的回归样例凑数。
Windows 命令行参数直接读取 Unicode 并转成 UTF-8，避免按本机代码页二次猜测编码。
标准输入由调用方提供 UTF-8。

若由其他 CMake 工程接入，可添加本目录并链接 `question_analyzer` 目标；
若手动编译，加入 `question_analyzer.cpp` 和本目录头文件搜索路径。`classifier_cli.cpp` 只供独立程序使用，不加入网关源文件列表，避免两个 `main` 冲突。
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

规则权重是项目人工预设值，不是概率。冻结题通过率只说明当前题集的表现。
当前只处理文档中列出的局部否定和成对引述，不理解任意嵌套引号、反讽、否定作用域、复杂多任务先后关系或完整自然语言语义；词典之外的表达仍可能误判。
多个证据分数不相同也不意味着主任务在语义上一定明确。发现新误判时，应补独立回归样例，并记录规则变更；修改冻结题的预期需单独说明原因和版本。
