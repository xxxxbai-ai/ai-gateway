# B 模块：公共接口与 V0.1 验收

更新：公共接口 1.0 已按 A 本次授权确定为接入基线，见 [统一约定](公共接口_v1.0.md)。
B 已采用 `shared/` 的唯一公共类型，原 `classification` 入口保留为别名。

日期：2026-10-26。代码位置：`modules/analyzer/` 与仓库根目录 `classifier.cpp`。
分类功能初始验证基线为 `51db28b`；公共接口发布分支基于 `ddedbd9`，包含已有 C/D 提交。
交付版本以包含本记录的 Git 提交为准；以下本地验证在提交前实际执行。

## 1. 公共接口如何确定

由 A 维护一份公共约定，B/C/D 分别核对自己接收和返回的数据，E 核对外部 JSON。
由 A 确定可编译的头文件和样例，记录接口版本及日期，再通知受影响成员按统一边界接入；后续改字段时同步记录影响。

当前 B 接口位于 `gateway_contract` 命名空间；下列仅为字段示意，正式类型以 `shared/contracts.h` 为准：

```cpp
namespace gateway_contract {
struct AnalysisResult {
    std::string task = "general";
    std::string reason;
    bool uncertain = true;
};
AnalysisResult analyzeQuestion(const std::string& question);
}
```

正式声明见 `shared/module_api.h`；`modules/analyzer/question_analyzer.h` 是兼容入口。

| 已确定的内容 | 当前公共接口 | 受影响成员 |
| --- | --- | --- |
| 问题输入 | UTF-8 `std::string`，A 在 HTTP 层拒绝空问题 | A、B |
| 任务名称 | `programming/math/translation/writing/general`，与现有 HTTP 文档及 C 路由一致 | A、B、C |
| 分类结果 | `task/reason/uncertain`；不确定时为 `general`，说明回退原因 | A、B、C |
| C/D 边界 | C 接任务与模型配置，D 接选定模型配置和问题；B 不包含模型凭证、调用或重试 | A、B、C、D |
| 现有框架类型 | 字符串与 `gateway::TaskType` 按名称映射，不能按枚举序号强转 | A、B；通知 C、D |
| Qt 返回格式 | 沿用现有 HTTP JSON；A 在 V0.2 将真实分类填入 `task/reason` | A、E |

公共接口 1.0 同时规定 C/D 的函数签名、模型配置和调用错误结构；本次 B 已适配，C/D 待各负责人实现或包装。
接口基线由 A 确定；本记录不代替其他成员的实现验收或签收。

## 2. 本次补齐的 B 任务

| V0.1 要求 | 实现位置与行为 |
| --- | --- |
| 纯 C++ 分类函数 | `question_analyzer.h/.cpp`，独立库 `question_analyzer` |
| 五类任务标准名称 | 公共返回值只使用现有约定的五个英文名称 |
| 可读判断理由 | 返回实际命中的关键词、结构规则、明确意图或回退原因 |
| 不确定标志 | 空白、无规则、冲突或证据不足时 `general/uncertain=true` |
| 可独立演示 | `analyzer_demo` 默认五类各一条，打印全部三个结果字段 |
| 保留原有自测 | `classifier --test` 使用同一公共接口检查原有 21 条样例 |
| 接口和异常边界 | 24 条标准输出及回退样例；并发检查；模块 README 说明输入前提和局限 |
| 交付说明 | 仓库与模块 README、本文的接口提案和验收记录 |

## 3. 实际运行记录

实际验证日期：2026-10-26。
环境：Windows、MinGW g++ 15.2.0、CMake 4.3.1、Ninja、C++17。
构建成功，开启 `-Wall -Wextra -Wpedantic` 后本次编译未出现警告。

本次从整个工作区目录执行，`team-repo/` 为共享仓库工作副本：

```powershell
cmake -S team-repo/modules/analyzer -B build/b-analyzer-v01 -G Ninja -DCMAKE_CXX_COMPILER=g++
cmake --build build/b-analyzer-v01
ctest --test-dir build/b-analyzer-v01 --output-on-failure --verbose
```

在共享仓库目录单独复现时，按 `modules/analyzer/README.md` 的命令运行。

| 检查 | 实际结果 |
| --- | --- |
| `analyzer_contract` | 24/24 条接口与回退样例通过 |
| 并发首次初始化与调用 | 8 个并发任务，共 128 次调用通过 |
| `classifier_regression` | 原有 21/21 条样例通过 |
| `analyzer_five_categories` | 五类各一条独立演示正常输出，见下表 |
| CTest 总结果 | 3/3 组通过，0 失败 |
| Windows 中文命令行 | 含引号及 emoji 的问题原文完整保留，正确返回翻译类型 |
| UTF-8 标准输入 | 数学题、无规则问题和空行逐行处理，结果及不确定标志符合约定 |

五类演示的实际输出摘要：

| 实际输入 | 实际 `task` | 实际 `uncertain` | 理由摘要 |
| --- | --- | --- | --- |
| 解释这段 C++ 代码报错 | `programming` | `false` | 命中代码、报错、c++；得分 9 |
| 计算256的平方根 | `math` | `false` | 命中计算、算、平方等规则；得分 8 |
| 把你好翻译成英文 | `translation` | `false` | 明确翻译意图；该类规则得分 14 |
| 帮我写一篇关于环保的作文 | `writing` | `false` | 明确写作意图，命中写一篇、作文；得分 6 |
| 为什么天空是蓝色的 | `general` | `false` | 命中为什么；通用得分 2 |

额外实际验证：“翻译这段 C++ 报错”返回 `translation/false`；
“你好”返回 `general/true`，理由为未命中专门规则且无明确通用问答线索。
24 条接口样例中还覆盖空白、专门任务同分、多意图、英文大小写及中性“帮我写”表达。

本地验证结论：B 的 V0.1 独立功能与交付材料已具备，可供其他成员复现和课上验收。
公共接口已统一，共享仓库提交及其他成员适配验收仍待实际完成；本次验证不代表全组 V0.1 或 V0.2 通过。

## 4. 范围与后续

- B 的独立代码交付与本地验证按本次记录验收；公共接口 1.0 已确定，课上签收和 GitHub 提交仍需实际完成并记录。
- 当前 HTTP 网关还没有接入本函数；这次没有把 `task:null` 改为真实分类。
- V0.2 再统一适配另一套框架的 `gateway::AnalysisResult`，完成 B/C/D 与 HTTP 网关串接。
- V0.2 的 30 条冻结题集仍需单独准备预期、固定版本和实际结果；本次样例不替代它。
- 原规则和新增保守回退只保证本次样例的行为，不能宣称适用于所有复杂问句。
